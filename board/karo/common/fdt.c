// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <console.h>
#include <env.h>
#include <fdt_support.h>
#include <fs.h>
#include <log.h>
#include <malloc.h>
#include <mtd_node.h>
#include <of_live.h>
#include <ubi_uboot.h>
#include <dm/of_access.h>
#include <linux/libfdt.h>
#include "karo.h"

#ifdef CONFIG_MAX_DTB_SIZE
#define MAX_DTB_SIZE	CONFIG_MAX_DTB_SIZE
#else
#define MAX_DTB_SIZE	SZ_64K
#endif

DECLARE_GLOBAL_DATA_PTR;

static void karo_set_fdtsize(void *fdt)
{
	size_t fdtsize = env_get_hex("fdtsize", 0);

	debug("%s@%d: fdtsize=%u\n", __func__, __LINE__, fdtsize);

	if (fdtsize == fdt_totalsize(fdt))
		return;

	debug("FDT size changed from %u to %u\n", fdtsize, fdt_totalsize(fdt));
	env_set_hex("fdtsize", fdt_totalsize(fdt));
}

static void *karo_fdt_load_dtb(unsigned long fdtaddr)
{
	int ret;
	void *fdt = (void *)fdtaddr;
	loff_t fdtsize;
	char *dtbfile;

	/* clear FDT header in memory */
	memset(fdt, 0, 4);

	if (had_ctrlc()) {
		printf("aborting DTB load\n");
		return NULL;
	}

#if CONFIG_IS_ENABLED(ENV_IS_IN_MMC)
	{
		const char *bootdev = env_get("bootdev");
		const char *bootpart = env_get("bootpart");

		dtbfile = env_get("dtbfile");
		debug("%s@%d:\n", __func__, __LINE__);

		if (!dtbfile) {
			debug("'dtbfile' not set; cannot load DTB\n");
			return NULL;
		}

		if (fs_set_blk_dev(bootdev, bootpart, FS_TYPE_ANY))
			return NULL;

		ret = fs_read(dtbfile, fdtaddr, 0, MAX_DTB_SIZE, &fdtsize);
		if (ret) {
			printf("Failed to load dtb from '%s': %d\n",
			       dtbfile, ret);
			return NULL;
		}
	}
#endif
#if CONFIG_IS_ENABLED(ENV_IS_IN_UBI)
	{
		dtbfile = "dtb";

		ret = ubi_volume_read(dtbfile, fdt, 0);
		if (ret) {
			printf("Failed to read UBI volume '%s': %d\n",
			       dtbfile, ret);
			return NULL;
		}
		fdtsize = env_get_hex("filesize", 0);
	}
#endif
	debug("FDT loaded from %s (%llu bytes)\n", dtbfile, fdtsize);
	if (fdt_check_header(fdt)) {
		debug("No valid DTB in flash\n");
		return NULL;
	}
	debug("Using DTB from flash @ %p\n", fdt);
	karo_set_fdtsize(fdt);
	return fdt;
}

void karo_fdt_move_fdt(void)
{
	void *fdt;
	unsigned long fdt_addr = env_get_ulong("fdtaddr", 16, 0);

	if (working_fdt) {
		debug("DTB already loaded\n");
		return;
	}

	env_set_hex("fdtsize", 0);

	if (!fdt_addr) {
		fdt_addr = CONFIG_SYS_FDT_ADDR;
		printf("fdtaddr is not set; using default: %08lx\n", fdt_addr);
	}

	fdt = karo_fdt_load_dtb(fdt_addr);
	if (!fdt) {
		fdt = (void *)gd->fdt_blob;
		if (!fdt) {
#ifdef CONFIG_OF_EMBED
			printf("Compiled in FDT not found");
#else
			printf("No FDT found");
#endif
			printf("; creating empty DTB\n");
			fdt = (void *)fdt_addr;
			fdt_create_empty_tree(fdt, 256);
		} else {
			printf("No DTB in flash; using default DTB\n");
		}
		debug("Checking FDT header @ %p\n", fdt);
		if (fdt_check_header(fdt)) {
			printf("ERROR: No valid DTB found at %p\n", fdt);
			return;
		}
		debug("Moving FDT from %p..%p to %08lx..%08lx\n",
		      fdt, fdt + fdt_totalsize(fdt) - 1,
		      fdt_addr, fdt_addr + fdt_totalsize(fdt) - 1);
		memmove((void *)fdt_addr, fdt, fdt_totalsize(fdt));
	}
	set_working_fdt_addr(fdt_addr);
	debug("fdt_addr set to %08lx\n", fdt_addr);

	karo_set_fdtsize(fdt);
}

#ifndef CONFIG_MTD
void karo_fixup_mtdparts(void *blob, struct node_info *info, size_t count)
{
	int root = fdt_path_offset(blob, "/soc");

	if (root <= 0)
		return;

	for (size_t i = 0; i < count; i++) {
		int node = fdt_node_offset_by_compatible(blob, root,
							 info[i].compat);

		if (!fdtdec_get_is_enabled(blob, node))
			continue;
		fdt_fixup_mtdparts(blob, &info[i], 1);
	}
}
#endif
