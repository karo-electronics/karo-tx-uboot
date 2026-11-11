// SPDX-License-Identifier: GPL-2.0+
/* Pick the carrier control DT from stored 'baseboard' */

#include <env.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <asm/global_data.h>
#include <linux/errno.h>
#include <linux/sizes.h>
#include <linux/libfdt.h>
#include <linux/string.h>
#include <u-boot/crc.h>

#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_KARO_DTB_SELECT_BUF_ADDR
#define CONFIG_KARO_DTB_SELECT_BUF_ADDR	CONFIG_SYS_LOAD_ADDR
#endif

#define KARO_ENV_BUF	((u8 *)(uintptr_t)CONFIG_KARO_DTB_SELECT_BUF_ADDR)

/* written before relocation: .data only */
static struct karo_dtb_state karo_dtb __section(".data");

/* Env block: crc, [flag], name=value list */
static int karo_env_parse(const u8 *buf, size_t size, bool redund,
			  char *out, size_t len)
{
	const char *p, *end;
	size_t hdr = 4 + (redund ? 1 : 0);
	u32 crc;

	memcpy(&crc, buf, sizeof(crc));
	if (crc32(0, buf + hdr, size - hdr) != crc)
		return -EBADMSG;

	end = (const char *)buf + size;
	for (p = (const char *)buf + hdr; p < end && *p;
	     p += strnlen(p, end - p) + 1) {
		if (strncmp(p, "baseboard=", 10))
			continue;
		strlcpy(out, p + 10, len);
		return 0;
	}
	return -ENOENT;
}

/* ---------------------------------------------------------------- MMC */
#if CONFIG_IS_ENABLED(ENV_IS_IN_MMC) || defined(CONFIG_ENV_IS_IN_MMC)
#include <blk.h>
#include <dm.h>
#include <mmc.h>

#if defined(CONFIG_XPL_BUILD) && !CONFIG_IS_ENABLED(ENV_IS_IN_MMC)
/* env/mmc.c helpers, not built for SPL here */
uint mmc_get_env_part(struct mmc *mmc)
{
#ifdef CONFIG_SYS_MMC_ENV_PART
	return CONFIG_SYS_MMC_ENV_PART;
#else
	return 0;
#endif
}

int mmc_get_env_addr(struct mmc *mmc, int copy, u32 *env_addr)
{
	*env_addr = CONFIG_ENV_OFFSET;
#ifdef CONFIG_ENV_OFFSET_REDUND
	if (copy)
		*env_addr = CONFIG_ENV_OFFSET_REDUND;
#endif
	return 0;
}
#endif

static int karo_env_baseboard_mmc(char *out, size_t len)
{
	const lbaint_t cnt = CONFIG_ENV_SIZE / 512;
	struct udevice *dev;
	struct mmc *mmc;
	struct blk_desc *desc;
	int copies = IS_ENABLED(CONFIG_SYS_REDUNDAND_ENVIRONMENT) ? 2 : 1;
	int prev_part, copy, ret;

	ret = uclass_get_device(UCLASS_MMC, mmc_get_env_dev(), &dev);
	if (ret)
		return ret;
	mmc = mmc_get_mmc_dev(dev);
	if (!mmc)
		return -ENODEV;

	/* Card uninitialised before relocation: use legacy timing */
	if (!mmc->has_init) {
		struct mmc_config *cfg = (struct mmc_config *)mmc->cfg;

		cfg->host_caps &= ~(MMC_MODE_HS | MMC_MODE_HS_52MHz |
				    MMC_MODE_HS200 | MMC_MODE_HS400 |
				    MMC_MODE_HS400_ES | MMC_MODE_DDR_52MHz |
				    UHS_CAPS);
		ret = mmc_init(mmc);
		if (ret)
			return ret;
	}
	desc = mmc_get_blk_desc(mmc);
	if (!desc)
		return -ENODEV;

	/* PARTITION_ACCESS is card state: remember and restore it */
	prev_part = desc->hwpart;
	/* no blk_select_hwpart_devnum(): it probes and scans partitions */
	ret = mmc_switch_part(mmc, mmc_get_env_part(mmc));
	if (ret)
		return ret;

	ret = -EBADMSG;
	for (copy = 0; copy < copies; copy++) {
		u32 addr;

		if (mmc_get_env_addr(mmc, copy, &addr))
			continue;
		if (blk_dread(desc, addr / 512, cnt, KARO_ENV_BUF) != cnt) {
			ret = -EIO;
			continue;
		}
		ret = karo_env_parse(KARO_ENV_BUF, CONFIG_ENV_SIZE,
				     copies == 2, out, len);
		if (ret != -EBADMSG)
			break;
	}

	/* Cache entries allocated here die at relocation: drop them. */
	/* Key by desc: the module DT may number the eMMC differently. */
	blkcache_invalidate(desc->uclass_id, desc->devnum);
	mmc_switch_part(mmc, prev_part);
	return ret;
}
#else
static int karo_env_baseboard_mmc(char *out, size_t len) { return -ENOSYS; }
#endif

/* --------------------------------------------------------------- NAND */
#if CONFIG_IS_ENABLED(ENV_IS_IN_NAND) || defined(CONFIG_ENV_IS_IN_NAND)
#include <nand.h>
#include <asm/sections.h>

#ifndef CONFIG_ENV_RANGE
#define CONFIG_ENV_RANGE	CONFIG_ENV_SIZE
#endif

/* .bss overlays .rel.dyn pre-relocation: park, zero, restore */
static u8 *const rel_dyn_park = KARO_ENV_BUF + CONFIG_ENV_SIZE + SZ_4K;

static bool karo_bss_quarantine_begin(void)
{
	size_t rel_len = __rel_dyn_end - __rel_dyn_start;
	size_t bss_len = __bss_end - __bss_start;

	if (gd->flags & GD_FLG_RELOC)
		return false;
	if ((uintptr_t)__bss_start != (uintptr_t)__rel_dyn_start ||
	    bss_len > rel_len)
		return false;	/* .bss reaches the appended FIT: refuse */
	memcpy(rel_dyn_park, __rel_dyn_start, rel_len);
	memset(__bss_start, 0, bss_len);
	return true;
}

static void karo_bss_quarantine_end(void)
{
	memcpy(__rel_dyn_start, rel_dyn_park, __rel_dyn_end - __rel_dyn_start);
}

static int karo_env_baseboard_nand(char *out, size_t len)
{
	struct mtd_info *mtd;
	int copies = IS_ENABLED(CONFIG_SYS_REDUNDAND_ENVIRONMENT) ? 2 : 1;
	loff_t offs[2] = { CONFIG_ENV_OFFSET,
#ifdef CONFIG_ENV_OFFSET_REDUND
			   CONFIG_ENV_OFFSET_REDUND
#else
			   0
#endif
	};
	int copy, ret = -EBADMSG;
	bool quarantined = karo_bss_quarantine_begin();

	if (!quarantined)
		return -ENOSYS;

	/* GPMI/BCH/APBH-DMA nodes must be bootph-all for this to bind */
	nand_init();
	mtd = get_nand_dev_by_index(0);
	if (!mtd) {
		ret = -ENODEV;
		goto out;
	}

	for (copy = 0; copy < copies; copy++) {
		size_t rd = CONFIG_ENV_SIZE;

		/* skips bad blocks inside the range like env/nand.c */
		if (nand_read_skip_bad(mtd, offs[copy], &rd, NULL,
				       CONFIG_ENV_RANGE, KARO_ENV_BUF) ||
		    rd != CONFIG_ENV_SIZE) {
			ret = -EIO;
			continue;
		}
		ret = karo_env_parse(KARO_ENV_BUF, CONFIG_ENV_SIZE,
				     copies == 2, out, len);
		if (ret != -EBADMSG)
			break;
	}
out:
	karo_bss_quarantine_end();
	return ret;
}
#else
static int karo_env_baseboard_nand(char *out, size_t len) { return -ENOSYS; }
#endif

/* ------------------------------------------------------------ SPI NOR */
#if CONFIG_IS_ENABLED(ENV_IS_IN_SPI_FLASH) || defined(CONFIG_ENV_IS_IN_SPI_FLASH)
static int karo_env_baseboard_spi(char *out, size_t len)
{
	/* CONFIG_ENV_SPI_EARLY: env_init() loaded the block, env_get_f works */
	if (!IS_ENABLED(CONFIG_ENV_SPI_EARLY))
		return -ENOSYS;
	if (env_get_f("baseboard", out, len) < 0)
		return -ENOENT;
	return 0;
}
#else
static int karo_env_baseboard_spi(char *out, size_t len) { return -ENOSYS; }
#endif

/* ------------------------------------------------------------ generic */
int karo_env_baseboard(char *out, size_t len)
{
	/* mfg/noenv images ignore the stored environment altogether */
	if (IS_ENABLED(CONFIG_UBOOT_IGNORE_ENV))
		return -ENOENT;
	if (IS_ENABLED(CONFIG_ENV_IS_IN_MMC))
		return karo_env_baseboard_mmc(out, len);
	if (IS_ENABLED(CONFIG_ENV_IS_IN_NAND))
		return karo_env_baseboard_nand(out, len);
	if (IS_ENABLED(CONFIG_ENV_IS_IN_SPI_FLASH))
		return karo_env_baseboard_spi(out, len);
	return -ENOSYS;
}

const struct karo_dtb_state *karo_dtb_select(void)
{
	struct karo_dtb_state *st = &karo_dtb;
	int ret;

	if (st->src != KARO_DTB_SRC_NONE)
		return st;

	ret = karo_env_baseboard(st->baseboard, sizeof(st->baseboard));
	st->read_err = ret;

	if (!ret && st->baseboard[0]) {
		st->src = KARO_DTB_SRC_STORED;
	} else if (CONFIG_KARO_DEFAULT_BASEBOARD[0]) {
		strlcpy(st->baseboard, CONFIG_KARO_DEFAULT_BASEBOARD,
			sizeof(st->baseboard));
		st->src = KARO_DTB_SRC_DEFAULT;
	} else {
		st->baseboard[0] = '\0';
		st->src = KARO_DTB_SRC_MODULE;
	}

	/* FIT configs are named by DT, no .dtb */
	snprintf(st->dt_name, sizeof(st->dt_name), "%s%s%s",
		 CONFIG_DEFAULT_DEVICE_TREE, st->baseboard[0] ? "-" : "",
		 st->baseboard);
	return st;
}

/* FIT config match, in SPL and U-Boot proper */
int board_fit_config_name_match(const char *name)
{
	const char *want = CONFIG_DEFAULT_DEVICE_TREE;

	if (IS_ENABLED(CONFIG_XPL_BUILD) || karo_dtb.src != KARO_DTB_SRC_NONE)
		want = karo_dtb_select()->dt_name;

	return strcmp(name, want) ? -EINVAL : 0;
}

#if CONFIG_IS_ENABLED(DTB_RESELECT) && !defined(CONFIG_XPL_BUILD)
/* Switch gd->fdt_blob only; dm_uninit() loses i.MX95 console */
int embedded_dtb_select(void)
{
	int rescan = 0;

	karo_dtb_select();
	fdtdec_resetup(&rescan);
	return 0;
}
#endif

#ifdef CONFIG_XPL_BUILD
#include <spl.h>
/* tell U-Boot proper which carrier SPL selected */
void spl_perform_fixups(struct spl_image_info *spl_image)
{
	void *fdt = spl_image->fdt_addr;
	const struct karo_dtb_state *st = karo_dtb_select();
	int node;

	if (!fdt || fdt_check_header(fdt))
		return;
	node = fdt_path_offset(fdt, "/chosen");
	if (node < 0)
		node = fdt_add_subnode(fdt, 0, "chosen");
	if (node >= 0)
		fdt_setprop_string(fdt, node, "karo,baseboard", st->baseboard);
}
#else
void karo_dtb_report(void)
{
	const struct karo_dtb_state *st = &karo_dtb;
	const char *model = fdt_getprop(gd->fdt_blob, 0, "model", NULL);
	const char *bb = NULL;
	static const char *const src_name[] = {
		"SPL", "stored environment", "built-in default", "none",
	};
	int node;

	if (st->src != KARO_DTB_SRC_NONE) {
		bb = st->baseboard;
	} else {
		node = fdt_path_offset(gd->fdt_blob, "/chosen");
		if (node >= 0)
			bb = fdt_getprop(gd->fdt_blob, node, "karo,baseboard",
					 NULL);
	}

	debug("Control DT: %s\n", model ?: "?");
	if (bb) {
		printf("Baseboard: %s\n", bb);
		debug("  baseboard from %s, read %d\n", src_name[st->src],
		      st->read_err);
	}
}
#endif
