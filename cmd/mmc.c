// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2003
 * Kyle Harris, kharris@nexus-tech.net
 */

#include <common.h>
#include <blk.h>
#include <command.h>
#include <console.h>
#include <memalign.h>
#include <mmc.h>
#include <part.h>
#include <sparse_format.h>
#include <image-sparse.h>

static int curr_device = -1;

static void print_mmcinfo(struct mmc *mmc)
{
	int i;

	printf("Device: %s\n", mmc->cfg->name);
	printf("Manufacturer ID: %x\n", mmc->cid[0] >> 24);
	if (IS_SD(mmc)) {
		printf("OEM: %x\n", (mmc->cid[0] >> 8) & 0xffff);
		printf("Name: %c%c%c%c%c \n", mmc->cid[0] & 0xff,
		       (mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
		       (mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff);
	} else {
		printf("OEM: %x\n", (mmc->cid[0] >> 8) & 0xff);
		printf("Name: %c%c%c%c%c%c \n", mmc->cid[0] & 0xff,
		       (mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
		       (mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff,
		       (mmc->cid[2] >> 24));
	}

	printf("Bus Speed: %d\n", mmc->clock);
#if CONFIG_IS_ENABLED(MMC_VERBOSE)
	printf("Mode: %s\n", mmc_mode_name(mmc->selected_mode));
	mmc_dump_capabilities("card capabilities", mmc->card_caps);
	mmc_dump_capabilities("host capabilities", mmc->host_caps);
#endif
	printf("Rd Block Len: %d\n", mmc->read_bl_len);

	printf("%s version %d.%d", IS_SD(mmc) ? "SD" : "MMC",
	       EXTRACT_SDMMC_MAJOR_VERSION(mmc->version),
	       EXTRACT_SDMMC_MINOR_VERSION(mmc->version));
	if (EXTRACT_SDMMC_CHANGE_VERSION(mmc->version) != 0)
		printf(".%d", EXTRACT_SDMMC_CHANGE_VERSION(mmc->version));
	printf("\n");

	printf("High Capacity: %s\n", mmc->high_capacity ? "Yes" : "No");
	puts("Capacity: ");
	print_size(mmc->capacity, "\n");

	printf("Bus Width: %d-bit%s\n", mmc->bus_width,
	       mmc->ddr_mode ? " DDR" : "");

#if CONFIG_IS_ENABLED(MMC_WRITE)
	puts("Erase Group Size: ");
	print_size(((u64)mmc->erase_grp_size) << 9, "\n");
#endif

	if (!IS_SD(mmc) && mmc->version >= MMC_VERSION_4_41) {
		bool has_enh = (mmc->part_support & ENHNCD_SUPPORT) != 0;
		bool usr_enh = has_enh && (mmc->part_attr & EXT_CSD_ENH_USR);
		ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);
		u8 wp;
		int ret;

#if CONFIG_IS_ENABLED(MMC_HW_PARTITIONING)
		puts("HC WP Group Size: ");
		print_size(((u64)mmc->hc_wp_grp_size) << 9, "\n");
#endif

		puts("User Capacity: ");
		print_size(mmc->capacity_user, usr_enh ? " ENH" : "");
		if (mmc->wr_rel_set & EXT_CSD_WR_DATA_REL_USR)
			puts(" WRREL\n");
		else
			putc('\n');
		if (usr_enh) {
			puts("User Enhanced Start: ");
			print_size(mmc->enh_user_start, "\n");
			puts("User Enhanced Size: ");
			print_size(mmc->enh_user_size, "\n");
		}
		puts("Boot Capacity: ");
		print_size(mmc->capacity_boot, has_enh ? " ENH\n" : "\n");
		puts("RPMB Capacity: ");
		print_size(mmc->capacity_rpmb, has_enh ? " ENH\n" : "\n");

		for (i = 0; i < ARRAY_SIZE(mmc->capacity_gp); i++) {
			bool is_enh = has_enh &&
				(mmc->part_attr & EXT_CSD_ENH_GP(i));
			if (mmc->capacity_gp[i]) {
				printf("GP%i Capacity: ", i+1);
				print_size(mmc->capacity_gp[i],
					   is_enh ? " ENH" : "");
				if (mmc->wr_rel_set & EXT_CSD_WR_DATA_REL_GP(i))
					puts(" WRREL\n");
				else
					putc('\n');
			}
		}
		ret = mmc_send_ext_csd(mmc, ext_csd);
		if (ret) {
			pr_err("Failed to read EXT_CSD: %d\n", ret);
			return;
		}

		wp = ext_csd[EXT_CSD_BOOT_WP_STATUS];
		for (i = 0; i < 2; ++i) {
			printf("Boot area %d is ", i);
			switch (wp & 3) {
			case 0:
				printf("not write protected\n");
				break;
			case 1:
				printf("power on protected\n");
				break;
			case 2:
				printf("permanently protected\n");
				break;
			default:
				printf("in reserved protection state\n");
				break;
			}
			wp >>= 2;
		}
	}
}

static struct mmc *__init_mmc_device(int dev, bool force_init,
				     enum bus_mode speed_mode)
{
	struct mmc *mmc;
	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("no mmc device at slot %x\n", dev);
		return NULL;
	}

	if (!mmc_getcd(mmc))
		force_init = true;

	if (force_init)
		mmc->has_init = 0;

	if (IS_ENABLED(CONFIG_MMC_SPEED_MODE_SET))
		mmc->user_speed_mode = speed_mode;

	if (mmc_init(mmc))
		return NULL;

#ifdef CONFIG_BLOCK_CACHE
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	blkcache_invalidate(bd->if_type, bd->devnum);
#endif

	return mmc;
}

static struct mmc *init_mmc_device(int dev, bool force_init)
{
	return __init_mmc_device(dev, force_init, MMC_MODES_END);
}

static int do_mmcinfo(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	struct mmc *mmc;

	if (curr_device < 0) {
		if (get_mmc_num() > 0)
			curr_device = 0;
		else {
			puts("No MMC device available\n");
			return 1;
		}
	}

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	print_mmcinfo(mmc);
	return CMD_RET_SUCCESS;
}

#if CONFIG_IS_ENABLED(CMD_MMC_RPMB)
static int confirm_key_prog(void)
{
	puts("Warning: Programming authentication key can be done only once !\n         Use this command only if you are sure of what you are doing,\nReally perform the key programming? <y/N> ");
	if (confirm_yesno())
		return 1;

	puts("Authentication key programming aborted\n");
	return 0;
}

static int do_mmcrpmb_key(struct cmd_tbl *cmdtp, int flag,
			  int argc, char *const argv[])
{
	void *key_addr;
	struct mmc *mmc = find_mmc_device(curr_device);

	if (argc != 2)
		return CMD_RET_USAGE;

	key_addr = (void *)hextoul(argv[1], NULL);
	if (!confirm_key_prog())
		return CMD_RET_FAILURE;
	if (mmc_rpmb_set_key(mmc, key_addr)) {
		printf("ERROR - Key already programmed ?\n");
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}

static int do_mmcrpmb_read(struct cmd_tbl *cmdtp, int flag,
			   int argc, char *const argv[])
{
	u16 blk, cnt;
	void *addr;
	int n;
	void *key_addr = NULL;
	struct mmc *mmc = find_mmc_device(curr_device);

	if (argc < 4)
		return CMD_RET_USAGE;

	addr = (void *)hextoul(argv[1], NULL);
	blk = hextoul(argv[2], NULL);
	cnt = hextoul(argv[3], NULL);

	if (argc == 5)
		key_addr = (void *)hextoul(argv[4], NULL);

	printf("\nMMC RPMB read: dev # %d, block # %d, count %d ... ",
	       curr_device, blk, cnt);
	n =  mmc_rpmb_read(mmc, addr, blk, cnt, key_addr);

	printf("%d RPMB blocks read: %s\n", n, (n == cnt) ? "OK" : "ERROR");
	if (n != cnt)
		return CMD_RET_FAILURE;
	return CMD_RET_SUCCESS;
}

static int do_mmcrpmb_write(struct cmd_tbl *cmdtp, int flag,
			    int argc, char *const argv[])
{
	u16 blk, cnt;
	void *addr;
	int n;
	void *key_addr;
	struct mmc *mmc = find_mmc_device(curr_device);

	if (argc != 5)
		return CMD_RET_USAGE;

	addr = (void *)hextoul(argv[1], NULL);
	blk = hextoul(argv[2], NULL);
	cnt = hextoul(argv[3], NULL);
	key_addr = (void *)hextoul(argv[4], NULL);

	printf("\nMMC RPMB write: dev # %d, block # %d, count %d ... ",
	       curr_device, blk, cnt);
	n =  mmc_rpmb_write(mmc, addr, blk, cnt, key_addr);

	printf("%d RPMB blocks written: %s\n", n, (n == cnt) ? "OK" : "ERROR");
	if (n != cnt)
		return CMD_RET_FAILURE;
	return CMD_RET_SUCCESS;
}

static int do_mmcrpmb_counter(struct cmd_tbl *cmdtp, int flag,
			      int argc, char *const argv[])
{
	unsigned long counter;
	struct mmc *mmc = find_mmc_device(curr_device);

	if (mmc_rpmb_get_counter(mmc, &counter))
		return CMD_RET_FAILURE;
	printf("RPMB Write counter= %lx\n", counter);
	return CMD_RET_SUCCESS;
}

static struct cmd_tbl cmd_rpmb[] = {
	U_BOOT_CMD_MKENT(key, 2, 0, do_mmcrpmb_key, "", ""),
	U_BOOT_CMD_MKENT(read, 5, 1, do_mmcrpmb_read, "", ""),
	U_BOOT_CMD_MKENT(write, 5, 0, do_mmcrpmb_write, "", ""),
	U_BOOT_CMD_MKENT(counter, 1, 1, do_mmcrpmb_counter, "", ""),
};

static int do_mmcrpmb(struct cmd_tbl *cmdtp, int flag,
		      int argc, char *const argv[])
{
	struct cmd_tbl *cp;
	struct mmc *mmc;
	char original_part;
	int ret;

	cp = find_cmd_tbl(argv[1], cmd_rpmb, ARRAY_SIZE(cmd_rpmb));

	/* Drop the rpmb subcommand */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (!(mmc->version & MMC_VERSION_MMC)) {
		printf("It is not an eMMC device\n");
		return CMD_RET_FAILURE;
	}
	if (mmc->version < MMC_VERSION_4_41) {
		printf("RPMB not supported before version 4.41\n");
		return CMD_RET_FAILURE;
	}
	/* Switch to the RPMB partition */
#ifndef CONFIG_BLK
	original_part = mmc->block_dev.hwpart;
#else
	original_part = mmc_get_blk_desc(mmc)->hwpart;
#endif
	if (blk_select_hwpart_devnum(IF_TYPE_MMC, curr_device, MMC_PART_RPMB) !=
	    0)
		return CMD_RET_FAILURE;
	ret = cp->cmd(cmdtp, flag, argc, argv);

	/* Return to original partition */
	if (blk_select_hwpart_devnum(IF_TYPE_MMC, curr_device, original_part) !=
	    0)
		return CMD_RET_FAILURE;
	return ret;
}
#endif

static int do_mmc_read(struct cmd_tbl *cmdtp, int flag,
		       int argc, char *const argv[])
{
	struct mmc *mmc;
	u32 blk, cnt, n;
	void *addr;

	if (argc != 4)
		return CMD_RET_USAGE;

	addr = (void *)hextoul(argv[1], NULL);
	blk = hextoul(argv[2], NULL);
	cnt = hextoul(argv[3], NULL);

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	printf("\nMMC read: dev # %d, block # %d, count %d ... ",
	       curr_device, blk, cnt);

	n = blk_dread(mmc_get_blk_desc(mmc), blk, cnt, addr);
	printf("%d blocks read: %s\n", n, (n == cnt) ? "OK" : "ERROR");

	return (n == cnt) ? CMD_RET_SUCCESS : CMD_RET_FAILURE;
}

#if CONFIG_IS_ENABLED(CMD_MMC_SWRITE)
static lbaint_t mmc_sparse_write(struct sparse_storage *info, lbaint_t blk,
				 lbaint_t blkcnt, const void *buffer)
{
	struct blk_desc *dev_desc = info->priv;

	return blk_dwrite(dev_desc, blk, blkcnt, buffer);
}

static lbaint_t mmc_sparse_reserve(struct sparse_storage *info,
				   lbaint_t blk, lbaint_t blkcnt)
{
	return blkcnt;
}

static int do_mmc_sparse_write(struct cmd_tbl *cmdtp, int flag,
			       int argc, char *const argv[])
{
	struct sparse_storage sparse;
	struct blk_desc *dev_desc;
	struct mmc *mmc;
	char dest[11];
	void *addr;
	u32 blk;

	if (argc != 3)
		return CMD_RET_USAGE;

	addr = (void *)hextoul(argv[1], NULL);
	blk = hextoul(argv[2], NULL);

	if (!is_sparse_image(addr)) {
		printf("Not a sparse image\n");
		return CMD_RET_FAILURE;
	}

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	printf("\nMMC Sparse write: dev # %d, block # %d ... ",
	       curr_device, blk);

	if (mmc_getwp(mmc) == 1) {
		printf("Error: card is write protected!\n");
		return CMD_RET_FAILURE;
	}

	dev_desc = mmc_get_blk_desc(mmc);
	sparse.priv = dev_desc;
	sparse.blksz = 512;
	sparse.start = blk;
	sparse.size = dev_desc->lba - blk;
	sparse.write = mmc_sparse_write;
	sparse.reserve = mmc_sparse_reserve;
	sparse.mssg = NULL;
	sprintf(dest, "0x" LBAF, sparse.start * sparse.blksz);

	if (write_sparse_image(&sparse, dest, addr, NULL))
		return CMD_RET_FAILURE;
	else
		return CMD_RET_SUCCESS;
}
#endif

#if CONFIG_IS_ENABLED(MMC_WRITE)
static int do_mmc_write(struct cmd_tbl *cmdtp, int flag,
			int argc, char *const argv[])
{
	struct mmc *mmc;
	u32 blk, cnt, n = 0;
	void *addr;
	int psa_mode = 0;
	int ret;

	if (argc < 4 || argc > 5)
		return CMD_RET_USAGE;

	if (argc == 5) {
		if (strncmp(argv[1], "-psa", strlen("-psa")))
			return CMD_RET_USAGE;
		n++;
		psa_mode = strcmp(argv[1], "-psa.auto") == 0 ?
			EXT_CSD_PSA_AUTO_PRE_SOLDERING : EXT_CSD_PSA_PRE_SOLDERING_WRITES;
	}
	addr = (void *)hextoul(argv[n + 1], NULL);
	blk = hextoul(argv[n + 2], NULL);
	cnt = hextoul(argv[n + 3], NULL);

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	printf("\nMMC write: dev # %d, block # %d, count %d ... ",
	       curr_device, blk, cnt);

	if (mmc_getwp(mmc) == 1) {
		printf("Error: card is write protected!\n");
		return CMD_RET_FAILURE;
	}

	struct blk_desc *blk_desc = mmc_get_blk_desc(mmc);
	unsigned long blksz;

	if (!blk_desc) {
		pr_err("Failed to get blk_desc for mmc dev\n");
		return CMD_RET_FAILURE;
	}
	if (psa_mode) {
		cnt /= 8;
		blksz = blk_desc->blksz;
		blk_desc->blksz = 4096;
		ret = mmc_set_psa(mmc, psa_mode, cnt);
		if (ret) {
			pr_err("Failed to set PSA_PRESOLDERING: %d\n", ret);
			ret = mmc_set_extcsd(mmc, EXT_CSD_PSA_ENABLEMENT, 0);
			if (ret)
				printf("Failed to reset PSA_ENABLEMENT}: %d\n", ret);
			ret = mmc_set_extcsd(mmc, EXT_CSD_PSA, EXT_CSD_PSA_NORMAL);
			if (ret)
				printf("Failed to reset PSA_MODE}: %d\n", ret);
			return CMD_RET_FAILURE;
		}
	}
	n = blk_dwrite(blk_desc, blk, cnt, addr);
	printf("%d blocks written: %s\n", n, (n == cnt) ? "OK" : "ERROR");

	if (psa_mode)
		blk_desc->blksz = blksz;

	if (psa_mode && n == cnt) {
		if (psa_mode == EXT_CSD_PSA_AUTO_PRE_SOLDERING) {
			ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);

			ret = mmc_send_ext_csd(mmc, ext_csd);
			if (ret) {
				pr_err("Failed to read EXT_CSD: %d\n", ret);
				return CMD_RET_FAILURE;
			}
			if (ext_csd[EXT_CSD_PSA] != EXT_CSD_PSA_NORMAL) {
				pr_err("EXT_CSD_PSA[%u] not reset to NORMAL after writing %u blocks\n",
				       ext_csd[EXT_CSD_PSA], cnt);
			}
		} else {
			ret = mmc_set_psa(mmc, EXT_CSD_PSA_PRE_SOLDERING_POST_WRITES, 0);
			if (ret)
				pr_err("Failed to set PSA_PRESOLDERING_POST_WRITES: %d\n", ret);
		}
	}
	return (n == cnt) ? CMD_RET_SUCCESS : CMD_RET_FAILURE;
}

static int do_mmc_erase(struct cmd_tbl *cmdtp, int flag,
			int argc, char *const argv[])
{
	struct mmc *mmc;
	u32 blk, cnt, n;

	if (argc != 3)
		return CMD_RET_USAGE;

	blk = hextoul(argv[1], NULL);
	cnt = hextoul(argv[2], NULL);

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	printf("\nMMC erase: dev # %d, block # %d, count %d ... ",
	       curr_device, blk, cnt);

	if (mmc_getwp(mmc) == 1) {
		printf("Error: card is write protected!\n");
		return CMD_RET_FAILURE;
	}
	n = blk_derase(mmc_get_blk_desc(mmc), blk, cnt);
	printf("%d blocks erased: %s\n", n, (n == cnt) ? "OK" : "ERROR");

	return (n == cnt) ? CMD_RET_SUCCESS : CMD_RET_FAILURE;
}
#endif

static int do_mmc_rescan(struct cmd_tbl *cmdtp, int flag,
			 int argc, char *const argv[])
{
	struct mmc *mmc;
	enum bus_mode speed_mode = MMC_MODES_END;

	if (argc == 1) {
		mmc = init_mmc_device(curr_device, true);
	} else if (argc == 2) {
		speed_mode = (int)dectoul(argv[1], NULL);
		mmc = __init_mmc_device(curr_device, true, speed_mode);
	} else {
		return CMD_RET_USAGE;
	}

	if (!mmc)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_mmc_part(struct cmd_tbl *cmdtp, int flag,
		       int argc, char *const argv[])
{
	struct blk_desc *mmc_dev;
	struct mmc *mmc;

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	mmc_dev = blk_get_devnum_by_type(IF_TYPE_MMC, curr_device);
	if (mmc_dev != NULL && mmc_dev->type != DEV_TYPE_UNKNOWN) {
		part_print(mmc_dev);
		return CMD_RET_SUCCESS;
	}

	puts("get mmc type error!\n");
	return CMD_RET_FAILURE;
}

static int do_mmc_dev(struct cmd_tbl *cmdtp, int flag,
		      int argc, char *const argv[])
{
	int dev, part = 0, ret;
	struct mmc *mmc;
	enum bus_mode speed_mode = MMC_MODES_END;

	if (argc == 1) {
		dev = curr_device;
		mmc = init_mmc_device(dev, true);
	} else if (argc == 2) {
		dev = (int)dectoul(argv[1], NULL);
		mmc = init_mmc_device(dev, true);
	} else if (argc == 3) {
		dev = (int)dectoul(argv[1], NULL);
		part = (int)dectoul(argv[2], NULL);
		if (part > PART_ACCESS_MASK) {
			printf("#part_num shouldn't be larger than %d\n",
			       PART_ACCESS_MASK);
			return CMD_RET_FAILURE;
		}
		mmc = init_mmc_device(dev, true);
	} else if (argc == 4) {
		dev = (int)dectoul(argv[1], NULL);
		part = (int)dectoul(argv[2], NULL);
		if (part > PART_ACCESS_MASK) {
			printf("#part_num shouldn't be larger than %d\n",
			       PART_ACCESS_MASK);
			return CMD_RET_FAILURE;
		}
		speed_mode = (int)dectoul(argv[3], NULL);
		mmc = __init_mmc_device(dev, true, speed_mode);
	} else {
		return CMD_RET_USAGE;
	}

	if (!mmc)
		return CMD_RET_FAILURE;

	ret = blk_select_hwpart_devnum(IF_TYPE_MMC, dev, part);
	printf("switch to partitions #%d, %s\n",
	       part, (!ret) ? "OK" : "ERROR");
	if (ret)
		return 1;

	curr_device = dev;
	if (mmc->part_config == MMCPART_NOAVAILABLE)
		printf("mmc%d is current device\n", curr_device);
	else
		printf("mmc%d(part %d) is current device\n",
		       curr_device, mmc_get_blk_desc(mmc)->hwpart);

	return CMD_RET_SUCCESS;
}

static int do_mmc_list(struct cmd_tbl *cmdtp, int flag,
		       int argc, char *const argv[])
{
	print_mmc_devices('\n');
	return CMD_RET_SUCCESS;
}

#if CONFIG_IS_ENABLED(MMC_HW_PARTITIONING)
static void parse_hwpart_user_enh_size(struct mmc *mmc,
				       struct mmc_hwpart_conf *pconf,
				       char *argv)
{
	int i, ret;

	pconf->user.enh_size = 0;

	if (!strcmp(argv, "-"))	{ /* The rest of eMMC */
		ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);
		ret = mmc_send_ext_csd(mmc, ext_csd);
		if (ret)
			return;
		/* The enh_size value is in 512B block units */
		pconf->user.enh_size =
			((ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT + 2] << 16) +
			 (ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT + 1] << 8) +
			 ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT]) * 1024 *
			ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] *
			ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
		pconf->user.enh_size -= pconf->user.enh_start;
		for (i = 0; i < ARRAY_SIZE(mmc->capacity_gp); i++) {
			/*
			 * If the eMMC already has GP partitions set,
			 * subtract their size from the maximum USER
			 * partition size.
			 *
			 * Else, if the command was used to configure new
			 * GP partitions, subtract their size from maximum
			 * USER partition size.
			 */
			if (mmc->capacity_gp[i]) {
				/* The capacity_gp is in 1B units */
				pconf->user.enh_size -= mmc->capacity_gp[i] >> 9;
			} else if (pconf->gp_part[i].size) {
				/* The gp_part[].size is in 512B units */
				pconf->user.enh_size -= pconf->gp_part[i].size;
			}
		}
	} else {
		pconf->user.enh_size = dectoul(argv, NULL);
	}
}

static int parse_hwpart_user(struct mmc *mmc, struct mmc_hwpart_conf *pconf,
			     int argc, char *const argv[])
{
	int i = 0;

	memset(&pconf->user, 0, sizeof(pconf->user));

	while (i < argc) {
		if (!strcmp(argv[i], "enh")) {
			if (i + 2 >= argc)
				return -1;
			pconf->user.enh_start =
				dectoul(argv[i + 1], NULL);
			parse_hwpart_user_enh_size(mmc, pconf, argv[i + 2]);
			i += 3;
		} else if (!strcmp(argv[i], "wrrel")) {
			if (i + 1 >= argc)
				return -1;
			pconf->user.wr_rel_change = 1;
			if (!strcmp(argv[i+1], "on"))
				pconf->user.wr_rel_set = 1;
			else if (!strcmp(argv[i+1], "off"))
				pconf->user.wr_rel_set = 0;
			else
				return -1;
			i += 2;
		} else {
			break;
		}
	}
	return i;
}

static int parse_hwpart_gp(struct mmc_hwpart_conf *pconf, int pidx,
			   int argc, char *const argv[])
{
	int i;

	memset(&pconf->gp_part[pidx], 0, sizeof(pconf->gp_part[pidx]));

	if (1 >= argc)
		return -1;
	pconf->gp_part[pidx].size = dectoul(argv[0], NULL);

	i = 1;
	while (i < argc) {
		if (!strcmp(argv[i], "enh")) {
			pconf->gp_part[pidx].enhanced = 1;
			i += 1;
		} else if (!strcmp(argv[i], "wrrel")) {
			if (i + 1 >= argc)
				return -1;
			pconf->gp_part[pidx].wr_rel_change = 1;
			if (!strcmp(argv[i+1], "on"))
				pconf->gp_part[pidx].wr_rel_set = 1;
			else if (!strcmp(argv[i+1], "off"))
				pconf->gp_part[pidx].wr_rel_set = 0;
			else
				return -1;
			i += 2;
		} else {
			break;
		}
	}
	return i;
}

static int do_mmc_hwpartition(struct cmd_tbl *cmdtp, int flag,
			      int argc, char *const argv[])
{
	struct mmc *mmc;
	struct mmc_hwpart_conf pconf = { };
	enum mmc_hwpart_conf_mode mode = MMC_HWPART_CONF_CHECK;
	int i, r, pidx;

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (IS_SD(mmc)) {
		puts("SD doesn't support partitioning\n");
		return CMD_RET_FAILURE;
	}

	if (argc < 1)
		return CMD_RET_USAGE;
	i = 1;
	while (i < argc) {
		if (!strcmp(argv[i], "user")) {
			i++;
			r = parse_hwpart_user(mmc, &pconf, argc - i, &argv[i]);
			if (r < 0)
				return CMD_RET_USAGE;
			i += r;
		} else if (!strncmp(argv[i], "gp", 2) &&
			   strlen(argv[i]) == 3 &&
			   argv[i][2] >= '1' && argv[i][2] <= '4') {
			pidx = argv[i][2] - '1';
			i++;
			r = parse_hwpart_gp(&pconf, pidx, argc-i, &argv[i]);
			if (r < 0)
				return CMD_RET_USAGE;
			i += r;
		} else if (!strcmp(argv[i], "check")) {
			mode = MMC_HWPART_CONF_CHECK;
			i++;
		} else if (!strcmp(argv[i], "set")) {
			mode = MMC_HWPART_CONF_SET;
			i++;
		} else if (!strcmp(argv[i], "complete")) {
			mode = MMC_HWPART_CONF_COMPLETE;
			i++;
		} else {
			return CMD_RET_USAGE;
		}
	}

	puts("Partition configuration:\n");
	if (pconf.user.enh_size) {
		puts("\tUser Enhanced Start: ");
		print_size(((u64)pconf.user.enh_start) << 9, "\n");
		puts("\tUser Enhanced Size: ");
		print_size(((u64)pconf.user.enh_size) << 9, "\n");
	} else {
		puts("\tNo enhanced user data area\n");
	}
	if (pconf.user.wr_rel_change)
		printf("\tUser partition write reliability: %s\n",
		       pconf.user.wr_rel_set ? "on" : "off");
	for (pidx = 0; pidx < 4; pidx++) {
		if (pconf.gp_part[pidx].size) {
			printf("\tGP%i Capacity: ", pidx+1);
			print_size(((u64)pconf.gp_part[pidx].size) << 9,
				   pconf.gp_part[pidx].enhanced ?
				   " ENH\n" : "\n");
		} else {
			printf("\tNo GP%i partition\n", pidx+1);
		}
		if (pconf.gp_part[pidx].wr_rel_change)
			printf("\tGP%i write reliability: %s\n", pidx+1,
			       pconf.gp_part[pidx].wr_rel_set ? "on" : "off");
	}

	if (!mmc_hwpart_config(mmc, &pconf, mode)) {
		if (mode == MMC_HWPART_CONF_COMPLETE)
			puts("Partitioning successful, power-cycle to make effective\n");
		return CMD_RET_SUCCESS;
	} else {
		puts("Failed!\n");
		return CMD_RET_FAILURE;
	}
}
#endif

#ifdef CONFIG_SUPPORT_EMMC_BOOT
static int do_mmc_bootbus(struct cmd_tbl *cmdtp, int flag,
			  int argc, char *const argv[])
{
	int dev;
	struct mmc *mmc;
	u8 width, reset, mode;

	if (argc != 5)
		return CMD_RET_USAGE;
	dev = dectoul(argv[1], NULL);
	width = dectoul(argv[2], NULL);
	reset = dectoul(argv[3], NULL);
	mode = dectoul(argv[4], NULL);

	mmc = init_mmc_device(dev, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (IS_SD(mmc)) {
		puts("BOOT_BUS_WIDTH only exists on eMMC\n");
		return CMD_RET_FAILURE;
	}

	/*
	 * BOOT_BUS_CONDITIONS[177]
	 * BOOT_MODE[4:3]
	 * 0x0 : Use SDR + Backward compatible timing in boot operation
	 * 0x1 : Use SDR + High Speed Timing in boot operation mode
	 * 0x2 : Use DDR in boot operation
	 * RESET_BOOT_BUS_CONDITIONS
	 * 0x0 : Reset bus width to x1, SDR, Backward compatible
	 * 0x1 : Retain BOOT_BUS_WIDTH and BOOT_MODE
	 * BOOT_BUS_WIDTH
	 * 0x0 : x1(sdr) or x4 (ddr) buswidth
	 * 0x1 : x4(sdr/ddr) buswith
	 * 0x2 : x8(sdr/ddr) buswith
	 *
	 */
	if (width >= 0x3) {
		printf("boot_bus_width %d is invalid\n", width);
		return CMD_RET_FAILURE;
	}

	if (reset >= 0x2) {
		printf("reset_boot_bus_width %d is invalid\n", reset);
		return CMD_RET_FAILURE;
	}

	if (mode >= 0x3) {
		printf("reset_boot_bus_width %d is invalid\n", mode);
		return CMD_RET_FAILURE;
	}

	/* acknowledge to be sent during boot operation */
	if (mmc_set_boot_bus_width(mmc, width, reset, mode)) {
		puts("BOOT_BUS_WIDTH is failed to change.\n");
		return CMD_RET_FAILURE;
	}

	printf("Set to BOOT_BUS_WIDTH = 0x%x, RESET = 0x%x, BOOT_MODE = 0x%x\n",
	       width, reset, mode);
	return CMD_RET_SUCCESS;
}

static int do_mmc_boot_resize(struct cmd_tbl *cmdtp, int flag,
			      int argc, char *const argv[])
{
	int dev;
	struct mmc *mmc;
	u32 bootsize, rpmbsize;

	if (argc != 4)
		return CMD_RET_USAGE;
	dev = dectoul(argv[1], NULL);
	bootsize = dectoul(argv[2], NULL);
	rpmbsize = dectoul(argv[3], NULL);

	mmc = init_mmc_device(dev, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (IS_SD(mmc)) {
		printf("It is not an eMMC device\n");
		return CMD_RET_FAILURE;
	}

	if (mmc_boot_partition_size_change(mmc, bootsize, rpmbsize)) {
		printf("EMMC boot partition Size change Failed.\n");
		return CMD_RET_FAILURE;
	}

	printf("EMMC boot partition Size %d MB\n", bootsize);
	printf("EMMC RPMB partition Size %d MB\n", rpmbsize);
	return CMD_RET_SUCCESS;
}

static int mmc_partconf_print(struct mmc *mmc, const char *varname)
{
	u8 ack, access, part;

	if (mmc->part_config == MMCPART_NOAVAILABLE) {
		printf("No part_config info for ver. 0x%x\n", mmc->version);
		return CMD_RET_FAILURE;
	}

	access = EXT_CSD_EXTRACT_PARTITION_ACCESS(mmc->part_config);
	ack = EXT_CSD_EXTRACT_BOOT_ACK(mmc->part_config);
	part = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);

	if (varname)
		env_set_hex(varname, part);

	printf("EXT_CSD[179], PARTITION_CONFIG:\nBOOT_ACK: 0x%x\nBOOT_PARTITION_ENABLE: 0x%x\nPARTITION_ACCESS: 0x%x\n",
	       ack, part, access);

	return CMD_RET_SUCCESS;
}

static int do_mmc_partconf(struct cmd_tbl *cmdtp, int flag,
			   int argc, char *const argv[])
{
	int dev;
	struct mmc *mmc;
	u8 ack, part_num, access;

	if (argc != 2 && argc != 3 && argc != 5)
		return CMD_RET_USAGE;

	dev = dectoul(argv[1], NULL);

	mmc = init_mmc_device(dev, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (IS_SD(mmc)) {
		puts("PARTITION_CONFIG only exists on eMMC\n");
		return CMD_RET_FAILURE;
	}

	if (argc == 2 || argc == 3)
		return mmc_partconf_print(mmc, argc == 3 ? argv[2] : NULL);

	ack = dectoul(argv[2], NULL);
	part_num = dectoul(argv[3], NULL);
	access = dectoul(argv[4], NULL);

	/* acknowledge to be sent during boot operation */
	return mmc_set_part_conf(mmc, ack, part_num, access);
}

static int do_mmc_rst_func(struct cmd_tbl *cmdtp, int flag,
			   int argc, char *const argv[])
{
	int dev;
	struct mmc *mmc;
	u8 enable;

	/*
	 * Set the RST_n_ENABLE bit of RST_n_FUNCTION
	 * The only valid values are 0x0, 0x1 and 0x2 and writing
	 * a value of 0x1 or 0x2 sets the value permanently.
	 */
	if (argc != 3)
		return CMD_RET_USAGE;

	dev = dectoul(argv[1], NULL);
	enable = dectoul(argv[2], NULL);

	if (enable > 2) {
		puts("Invalid RST_n_ENABLE value\n");
		return CMD_RET_USAGE;
	}

	mmc = init_mmc_device(dev, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (IS_SD(mmc)) {
		puts("RST_n_FUNCTION only exists on eMMC\n");
		return CMD_RET_FAILURE;
	}

	return mmc_set_rst_n_function(mmc, enable);
}
#endif

static inline unsigned int get_extcsd_val32(u8 *ext_csd, int index)
{
	return (ext_csd[index + 3] << 24) |
		(ext_csd[index + 2] << 16) |
		(ext_csd[index + 1] << 8)  |
		ext_csd[index];
}

static inline unsigned int get_sector_count(u8 *ext_csd)
{
	return get_extcsd_val32(ext_csd, EXT_CSD_SEC_COUNT_0);
}

static inline int is_blockaddresed(u8 *ext_csd)
{
	unsigned int sectors = get_sector_count(ext_csd);

	/* over 2GiB devices are block-addressed */
	return (sectors > (2u * 1024 * 1024 * 1024) / 512);
}

static void print_writeprotect_boot_status(u8 *ext_csd)
{
	u8 reg;
	u8 ext_csd_rev = ext_csd[EXT_CSD_REV];

	/* A43: reserved [174:0] */
	if (ext_csd_rev >= 5) {
		printf("Boot write protection status registers [BOOT_WP_STATUS]: 0x%02x\n", ext_csd[174]);

		reg = ext_csd[EXT_CSD_BOOT_WP];
		printf("Boot Area Write protection [BOOT_WP]: 0x%02x\n", reg);
		printf(" Power ro locking: ");
		if (reg & EXT_CSD_BOOT_WP_B_PWR_WP_DIS)
			printf("not possible\n");
		else
			printf("possible\n");

		printf(" Permanent ro locking: ");
		if (reg & EXT_CSD_BOOT_WP_B_PERM_WP_DIS)
			printf("not possible\n");
		else
			printf("possible\n");

		reg = ext_csd[EXT_CSD_BOOT_WP_STATUS];
		printf(" partition 0 ro lock status: ");
		if (reg & EXT_CSD_BOOT_WP_S_AREA_0_PERM)
			printf("locked permanently\n");
		else if (reg & EXT_CSD_BOOT_WP_S_AREA_0_PWR)
			printf("locked until next power on\n");
		else
			printf("not locked\n");
		printf(" partition 1 ro lock status: ");
		if (reg & EXT_CSD_BOOT_WP_S_AREA_1_PERM)
			printf("locked permanently\n");
		else if (reg & EXT_CSD_BOOT_WP_S_AREA_1_PWR)
			printf("locked until next power on\n");
		else
			printf("not locked\n");
	}
}

void mmc_extcsd_print(u8 *ext_csd)
{
	u8 ext_csd_rev;
	u8 reg;
	u32 regl;
	const char *str;

	ext_csd_rev = ext_csd[EXT_CSD_REV];

	switch (ext_csd_rev) {
	case 8:
		str = "5.1";
		break;
	case 7:
		str = "5.0";
		break;
	case 6:
		str = "4.5";
		break;
	case 5:
		str = "4.41";
		break;
	case 3:
		str = "4.3";
		break;
	case 2:
		str = "4.2";
		break;
	case 1:
		str = "4.1";
		break;
	case 0:
		str = "4.0";
		break;
	default:
		return;
	}
	printf("=============================================\n");
	printf("  Extended CSD rev 1.%d (MMC %s)\n", ext_csd_rev, str);
	printf("=============================================\n\n");

	if (ext_csd_rev < 3)
		return;

	/* Parse the Extended CSD registers.
	 * Reserved bit should be read as "0" in case of spec older
	 * than A441.
	 */
	reg = ext_csd[EXT_CSD_S_CMD_SET];
	printf("Card Supported Command sets [S_CMD_SET: 0x%02x]\n", reg);
	if (!reg)
		printf(" - Standard MMC command sets\n");

	reg = ext_csd[EXT_CSD_HPI_FEATURE];
	printf("HPI Features [HPI_FEATURE: 0x%02x]: ", reg);
	if (reg & EXT_CSD_HPI_SUPP) {
		if (reg & EXT_CSD_HPI_IMPL)
			printf("implementation based on CMD12\n");
		else
			printf("implementation based on CMD13\n");
	}

	printf("Background operations support [BKOPS_SUPPORT: 0x%02x]\n",
	       ext_csd[502]);

	if (ext_csd_rev >= 6) {
		printf("Max Packet Read Cmd [MAX_PACKED_READS: 0x%02x]\n",
		       ext_csd[501]);
		printf("Max Packet Write Cmd [MAX_PACKED_WRITES: 0x%02x]\n",
		       ext_csd[500]);
		printf("Data TAG support [DATA_TAG_SUPPORT: 0x%02x]\n",
		       ext_csd[499]);

		printf("Data TAG Unit Size [TAG_UNIT_SIZE: 0x%02x]\n",
		       ext_csd[498]);
		printf("Tag Resources Size [TAG_RES_SIZE: 0x%02x]\n",
		       ext_csd[497]);
		printf("Context Management Capabilities [CONTEXT_CAPABILITIES: 0x%02x]\n", ext_csd[496]);
		printf("Large Unit Size [LARGE_UNIT_SIZE_M1: 0x%02x]\n",
		       ext_csd[495]);
		printf("Extended partition attribute support [EXT_SUPPORT: 0x%02x]\n", ext_csd[494]);
		printf("Generic CMD6 Timer [GENERIC_CMD6_TIME: 0x%02x]\n",
		       ext_csd[248]);
		printf("Power off notification [POWER_OFF_LONG_TIME: 0x%02x]\n",
		       ext_csd[247]);
		printf("Cache Size [CACHE_SIZE] is %d KiB\n",
		       (ext_csd[249] << 0 | (ext_csd[250] << 8) |
			(ext_csd[251] << 16) | (ext_csd[252] << 24)) / 8);
	}

	/* A441: Reserved [501:247]
	   A43: reserved [246:229] */
	if (ext_csd_rev >= 5) {
		printf("Background operations status [BKOPS_STATUS: 0x%02x]\n", ext_csd[246]);

		/* CORRECTLY_PRG_SECTORS_NUM [245:242] TODO */

		printf("1st Initialisation Time after programmed sector [INI_TIMEOUT_AP: 0x%02x]\n", ext_csd[241]);

		/* A441: reserved [240] */
		printf("Power class for 52MHz, DDR at 3.6V [PWR_CL_DDR_52_360: 0x%02x]\n", ext_csd[239]);
		printf("Power class for 52MHz, DDR at 1.95V [PWR_CL_DDR_52_195: 0x%02x]\n", ext_csd[238]);

		/* A441: reserved [237-236] */

		if (ext_csd_rev >= 6) {
			printf("Power class for 200MHz at 3.6V [PWR_CL_200_360: 0x%02x]\n", ext_csd[237]);
			printf("Power class for 200MHz, at 1.95V [PWR_CL_200_195: 0x%02x]\n", ext_csd[236]);
		}
		printf("Minimum Performance for 8bit at 52MHz in DDR mode:\n");
		printf(" [MIN_PERF_DDR_W_8_52: 0x%02x]\n", ext_csd[235]);
		printf(" [MIN_PERF_DDR_R_8_52: 0x%02x]\n", ext_csd[234]);
		/* A441: reserved [233] */
		printf("TRIM Multiplier [TRIM_MULT: 0x%02x]\n", ext_csd[232]);
		printf("Secure Feature support [SEC_FEATURE_SUPPORT: 0x%02x]\n",
		       ext_csd[231]);
	}
	if (ext_csd_rev == 5) { /* Obsolete in 4.5 */
		printf("Secure Erase Multiplier [SEC_ERASE_MULT: 0x%02x]\n",
		       ext_csd[230]);
		printf("Secure TRIM Multiplier [SEC_TRIM_MULT: 0x%02x]\n",
		       ext_csd[229]);
	}
	reg = ext_csd[EXT_CSD_BOOT_INFO];
	printf("Boot Information [BOOT_INFO: 0x%02x]\n", reg);
	if (reg & EXT_CSD_BOOT_INFO_ALT)
		printf(" Device supports alternative boot method\n");
	if (reg & EXT_CSD_BOOT_INFO_DDR_DDR)
		printf(" Device supports dual data rate during boot\n");
	if (reg & EXT_CSD_BOOT_INFO_HS_MODE)
		printf(" Device supports high speed timing during boot\n");

	/* A441/A43: reserved [227] */
	printf("Boot partition size [BOOT_SIZE_MULTI: 0x%02x]\n", ext_csd[226]);
	printf("Access size [ACC_SIZE: 0x%02x]\n", ext_csd[225]);

	reg = ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
	printf("High-capacity erase unit size [HC_ERASE_GRP_SIZE: 0x%02x]\n",
	       reg);
	printf(" i.e. %u KiB\n", 512 * reg);

	printf("High-capacity erase timeout [ERASE_TIMEOUT_MULT: 0x%02x]\n",
	       ext_csd[223]);
	printf("Reliable write sector count [REL_WR_SEC_C: 0x%02x]\n",
	       ext_csd[222]);

	reg = ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
	printf("High-capacity W protect group size [HC_WP_GRP_SIZE: 0x%02x]\n",
	       reg);
	printf(" i.e. %lu KiB\n", 512l * ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] * reg);

	printf("Sleep current (VCC) [S_C_VCC: 0x%02x]\n", ext_csd[220]);
	printf("Sleep current (VCCQ) [S_C_VCCQ: 0x%02x]\n", ext_csd[219]);
	/* A441/A43: reserved [218] */
	printf("Sleep/awake timeout [S_A_TIMEOUT: 0x%02x]\n", ext_csd[217]);
	/* A441/A43: reserved [216] */

	unsigned int sectors =	get_sector_count(ext_csd);
	printf("Sector Count [SEC_COUNT: 0x%08x]\n", sectors);
	if (is_blockaddresed(ext_csd))
		printf(" Device is block-addressed\n");
	else
		printf(" Device is NOT block-addressed\n");

	/* A441/A43: reserved [211] */
	printf("Minimum Write Performance for 8bit:\n");
	printf(" [MIN_PERF_W_8_52: 0x%02x]\n", ext_csd[210]);
	printf(" [MIN_PERF_R_8_52: 0x%02x]\n", ext_csd[209]);
	printf(" [MIN_PERF_W_8_26_4_52: 0x%02x]\n", ext_csd[208]);
	printf(" [MIN_PERF_R_8_26_4_52: 0x%02x]\n", ext_csd[207]);
	printf("Minimum Write Performance for 4bit:\n");
	printf(" [MIN_PERF_W_4_26: 0x%02x]\n", ext_csd[206]);
	printf(" [MIN_PERF_R_4_26: 0x%02x]\n", ext_csd[205]);
	/* A441/A43: reserved [204] */
	printf("Power classes registers:\n");
	printf(" [PWR_CL_26_360: 0x%02x]\n", ext_csd[203]);
	printf(" [PWR_CL_52_360: 0x%02x]\n", ext_csd[202]);
	printf(" [PWR_CL_26_195: 0x%02x]\n", ext_csd[201]);
	printf(" [PWR_CL_52_195: 0x%02x]\n", ext_csd[200]);

	/* A43: reserved [199:198] */
	if (ext_csd_rev >= 5) {
		printf("Partition switching timing [PARTITION_SWITCH_TIME: 0x%02x]\n", ext_csd[199]);
		printf("Out-of-interrupt busy timing [OUT_OF_INTERRUPT_TIME: 0x%02x]\n", ext_csd[198]);
	}

	/* A441/A43: reserved	[197] [195] [193] [190] [188]
	 * [186] [184] [182] [180] [176] */

	if (ext_csd_rev >= 6)
		printf("I/O Driver Strength [DRIVER_STRENGTH: 0x%02x]\n",
		       ext_csd[197]);

	/* DEVICE_TYPE in A45, CARD_TYPE in A441 */
	reg = ext_csd[196];
	printf("Card Type [CARD_TYPE: 0x%02x]\n", reg);
	if (reg & 0x80)
		printf(" HS400 Dual Data Rate eMMC @200MHz 1.2VI/O\n");
	if (reg & 0x40)
		printf(" HS400 Dual Data Rate eMMC @200MHz 1.8VI/O\n");
	if (reg & 0x20)
		printf(" HS200 Single Data Rate eMMC @200MHz 1.2VI/O\n");
	if (reg & 0x10)
		printf(" HS200 Single Data Rate eMMC @200MHz 1.8VI/O\n");
	if (reg & 0x08)
		printf(" HS Dual Data Rate eMMC @52MHz 1.2VI/O\n");
	if (reg & 0x04)
		printf(" HS Dual Data Rate eMMC @52MHz 1.8V or 3VI/O\n");
	if (reg & 0x02)
		printf(" HS eMMC @52MHz - at rated device voltage(s)\n");
	if (reg & 0x01)
		printf(" HS eMMC @26MHz - at rated device voltage(s)\n");

	printf("CSD structure version [CSD_STRUCTURE: 0x%02x]\n", ext_csd[194]);
	/* ext_csd_rev = ext_csd[EXT_CSD_REV] (already done!!!) */
	printf("Command set [CMD_SET: 0x%02x]\n", ext_csd[191]);
	printf("Command set revision [CMD_SET_REV: 0x%02x]\n", ext_csd[189]);
	printf("Power class [POWER_CLASS: 0x%02x]\n", ext_csd[187]);
	printf("High-speed interface timing [HS_TIMING: 0x%02x]\n",
	       ext_csd[185]);
	if (ext_csd_rev >= 8)
		printf("Enhanced Strobe mode [STROBE_SUPPORT: 0x%02x]\n",
		       ext_csd[184]);
	/* bus_width: ext_csd[183] not readable */
	printf("Erased memory content [ERASED_MEM_CONT: 0x%02x]\n",
	       ext_csd[181]);
	reg = ext_csd[EXT_CSD_BOOT_CFG];
	printf("Boot configuration bytes [PARTITION_CONFIG: 0x%02x]\n", reg);
	switch ((reg & EXT_CSD_BOOT_CFG_EN) >> 3) {
	case 0x0:
		printf(" Not boot enable\n");
		break;
	case 0x1:
		printf(" Boot Partition 1 enabled\n");
		break;
	case 0x2:
		printf(" Boot Partition 2 enabled\n");
		break;
	case 0x7:
		printf(" User Area Enabled for boot\n");
		break;
	}
	switch (reg & EXT_CSD_BOOT_CFG_ACC) {
	case 0x0:
		printf(" No access to boot partition\n");
		break;
	case 0x1:
		printf(" R/W Boot Partition 1\n");
		break;
	case 0x2:
		printf(" R/W Boot Partition 2\n");
		break;
	case 0x3:
		printf(" R/W Replay Protected Memory Block (RPMB)\n");
		break;
	default:
		printf(" Access to General Purpose partition %d\n",
		       (reg & EXT_CSD_BOOT_CFG_ACC) - 3);
		break;
	}

	printf("Boot config protection [BOOT_CONFIG_PROT: 0x%02x]\n",
	       ext_csd[178]);
	printf("Boot bus Conditions [BOOT_BUS_CONDITIONS: 0x%02x]\n",
	       ext_csd[177]);
	printf("High-density erase group definition [ERASE_GROUP_DEF: 0x%02x]\n", ext_csd[EXT_CSD_ERASE_GROUP_DEF]);

	print_writeprotect_boot_status(ext_csd);

	if (ext_csd_rev >= 5) {
		int i;
		const char * const fast = "existing data is at risk if a power failure occurs during a write operation";
		const char * const reliable = "the device protects existing data if a power failure occurs during a write operation";
		unsigned int wp_sz = ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
		unsigned int erase_sz = ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];

		/* A441]: reserved [172] */
		printf("User area write protection register [USER_WP]: 0x%02x\n", ext_csd[171]);
		/* A441]: reserved [170] */
		printf("FW configuration [FW_CONFIG]: 0x%02x\n", ext_csd[169]);
		printf("RPMB Size [RPMB_SIZE_MULT]: 0x%02x\n", ext_csd[168]);

		reg = ext_csd[EXT_CSD_WR_REL_SET];
		printf("Write reliability setting register [WR_REL_SET]: 0x%02x\n", reg);
		printf(" user area: %s\n", (reg & BIT(0)) ? reliable : fast);

		for (i = 1; i <= 4; i++)
			printf(" partition %d: %s\n", i,
			       (reg & BIT(i)) ? reliable : fast);

		reg = ext_csd[EXT_CSD_WR_REL_PARAM];
		printf("Write reliability parameter register [WR_REL_PARAM]: 0x%02x\n", reg);
		if (reg & 0x01)
			printf(" Device supports writing EXT_CSD_WR_REL_SET\n");
		if (reg & 0x04)
			printf(" Device supports the enhanced def. of reliable write\n");

		/* sanitize_start ext_csd[165]]: not readable
		 * bkops_start ext_csd[164]]: only writable */
		printf("Enable background operations handshake [BKOPS_EN]: 0x%02x\n", ext_csd[163]);
		printf("H/W reset function [RST_N_FUNCTION]: 0x%02x\n", ext_csd[162]);
		printf("HPI management [HPI_MGMT]: 0x%02x\n", ext_csd[161]);
		reg = ext_csd[EXT_CSD_PARTITIONING_SUPPORT];
		printf("Partitioning Support [PARTITIONING_SUPPORT]: 0x%02x\n",
		       reg);
		if (reg & EXT_CSD_PARTITIONING_EN)
			printf(" Device support partitioning feature\n");
		else
			printf(" Device NOT support partitioning feature\n");
		if (reg & EXT_CSD_ENH_ATTRIBUTE_EN)
			printf(" Device can have enhanced tech.\n");
		else
			printf(" Device cannot have enhanced tech.\n");

		regl = (ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT_2] << 16) |
			(ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT_1] << 8) |
			ext_csd[EXT_CSD_MAX_ENH_SIZE_MULT_0];

		printf("Max Enhanced Area Size [MAX_ENH_SIZE_MULT]: 0x%06x\n",
		       regl);

		printf(" i.e. %lu KiB\n", 512l * regl * wp_sz * erase_sz);

		printf("Partitions attribute [PARTITIONS_ATTRIBUTE]: 0x%02x\n",
		       ext_csd[EXT_CSD_PARTITIONS_ATTRIBUTE]);
		reg = ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED];
		printf("Partitioning Setting [PARTITION_SETTING_COMPLETED]: 0x%02x\n",
		       reg);
		if (reg)
			printf(" Device partition setting complete\n");
		else
			printf(" Device partition setting NOT complete\n");

		printf("General Purpose Partition Size\n [GP_SIZE_MULT_4]: 0x%06x\n", (ext_csd[154] << 16) |
		       (ext_csd[153] << 8) | ext_csd[152]);
		printf(" [GP_SIZE_MULT_3]: 0x%06x\n", (ext_csd[151] << 16) |
		       (ext_csd[150] << 8) | ext_csd[149]);
		printf(" [GP_SIZE_MULT_2]: 0x%06x\n", (ext_csd[148] << 16) |
		       (ext_csd[147] << 8) | ext_csd[146]);
		printf(" [GP_SIZE_MULT_1]: 0x%06x\n", (ext_csd[145] << 16) |
		       (ext_csd[144] << 8) | ext_csd[143]);

		regl =	(ext_csd[EXT_CSD_ENH_SIZE_MULT_2] << 16) |
			(ext_csd[EXT_CSD_ENH_SIZE_MULT_1] << 8) |
			ext_csd[EXT_CSD_ENH_SIZE_MULT_0];

		printf("Enhanced User Data Area Size [ENH_SIZE_MULT]: 0x%06x\n", regl);
		printf(" i.e. %lu KiB\n", 512l * regl *
		       ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] *
		       ext_csd[EXT_CSD_HC_WP_GRP_SIZE]);

		regl =	(ext_csd[EXT_CSD_ENH_START_ADDR_3] << 24) |
			(ext_csd[EXT_CSD_ENH_START_ADDR_2] << 16) |
			(ext_csd[EXT_CSD_ENH_START_ADDR_1] << 8) |
			ext_csd[EXT_CSD_ENH_START_ADDR_0];
		printf("Enhanced User Data Start Address [ENH_START_ADDR]: 0x%08x\n", regl);
		printf(" i.e. %llu bytes offset\n", (is_blockaddresed(ext_csd) ?
						     512ll : 1ll) * regl);

		/* A441]: reserved [135] */
		printf("Bad Block Management mode [SEC_BAD_BLK_MGMNT]: 0x%02x\n", ext_csd[134]);
		/* A441: reserved [133:0] */
	}
	/* B45 */
	if (ext_csd_rev >= 6) {
		int j;

		/* tcase_support ext_csd[132] not readable */
		printf("Periodic Wake-up [PERIODIC_WAKEUP]: 0x%02x\n",
		       ext_csd[131]);
		printf("Program CID/CSD in DDR mode support [PROGRAM_CID_CSD_DDR_SUPPORT]: 0x%02x\n",
		       ext_csd[130]);

		for (j = 127; j >= 64; j--)
			printf("Vendor Specific Fields [VENDOR_SPECIFIC_FIELD[%d]]: 0x%02x\n",
			       j, ext_csd[j]);

		printf("Native sector size [NATIVE_SECTOR_SIZE]: 0x%02x\n",
		       ext_csd[63]);
		printf("Sector size emulation [USE_NATIVE_SECTOR]: 0x%02x\n",
		       ext_csd[62]);
		printf("Sector size [DATA_SECTOR_SIZE]: 0x%02x\n", ext_csd[61]);
		printf("1st initialization after disabling sector size emulation [INI_TIMEOUT_EMU]: 0x%02x\n",
		       ext_csd[60]);
		printf("Class 6 commands control [CLASS_6_CTRL]: 0x%02x\n",
		       ext_csd[59]);
		printf("Number of addressed group to be Released[DYNCAP_NEEDED]: 0x%02x\n", ext_csd[58]);
		printf("Exception events control [EXCEPTION_EVENTS_CTRL]: 0x%04x\n",
		       (ext_csd[57] << 8) | ext_csd[56]);
		printf("Exception events status[EXCEPTION_EVENTS_STATUS]: 0x%04x\n",
		       (ext_csd[55] << 8) | ext_csd[54]);
		printf("Extended Partitions Attribute [EXT_PARTITIONS_ATTRIBUTE]: 0x%04x\n",
		       (ext_csd[53] << 8) | ext_csd[52]);

		for (j = 51; j >= 37; j--)
			printf("Context configuration [CONTEXT_CONF[%d]]: 0x%02x\n", j, ext_csd[j]);

		printf("Packed command status [PACKED_COMMAND_STATUS]: 0x%02x\n", ext_csd[36]);
		printf("Packed command failure index [PACKED_FAILURE_INDEX]: 0x%02x\n", ext_csd[35]);
		printf("Power Off Notification [POWER_OFF_NOTIFICATION]: 0x%02x\n", ext_csd[34]);
		printf("Control to turn the Cache ON/OFF [CACHE_CTRL]: 0x%02x\n", ext_csd[33]);
		/* flush_cache ext_csd[32] not readable */
		printf("Control to turn the Cache Barrier ON/OFF [BARRIER_CTRL]: 0x%02x\n", ext_csd[31]);
		/*Reserved [30:0] */
	}

	if (ext_csd_rev >= 7) {
		printf("eMMC Firmware Version: %.8s\n", (char *)&ext_csd[EXT_CSD_FIRMWARE_VERSION]);
		printf("eMMC Life Time Estimation A [EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_A]: 0x%02x\n",
		       ext_csd[EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_A]);
		printf("eMMC Life Time Estimation B [EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_B]: 0x%02x\n",
		       ext_csd[EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_B]);
		printf("eMMC Pre EOL information [EXT_CSD_PRE_EOL_INFO]: 0x%02x\n",
		       ext_csd[EXT_CSD_PRE_EOL_INFO]);
		reg = ext_csd[EXT_CSD_SECURE_REMOVAL_TYPE];
		printf("Secure Removal Type [SECURE_REMOVAL_TYPE]: 0x%02x\n", reg);
		printf(" information is configured to be removed ");
		/* Bit [5:4]: Configure Secure Removal Type */
		switch ((reg & EXT_CSD_CONFIG_SECRM_TYPE) >> 4) {
		case 0x0:
			printf("by an erase of the physical memory\n");
			break;
		case 0x1:
			printf("by overwriting the addressed locations with a character followed by an erase\n");
			break;
		case 0x2:
			printf("by overwriting the addressed locations with a character, its complement, then a random character\n");
			break;
		case 0x3:
			printf("using a vendor defined\n");
		}
		/* Bit [3:0]: Supported Secure Removal Type */
		printf(" Supported Secure Removal Type:\n");
		if (reg & 0x01)
			printf("  information removed by an erase of the physical memory\n");
		if (reg & 0x02)
			printf("  information removed by overwriting the addressed locations with a character followed by an erase\n");
		if (reg & 0x04)
			printf("  information removed by overwriting the addressed locations with a character, its complement, then a random character\n");
		if (reg & 0x08)
			printf("  information removed using a vendor defined\n");

		printf("Production State Awareness:\n");
		printf("Enablement [PSA_ENABLEMENT[%u]]: 0x%02x\n",
		       EXT_CSD_PSA_ENABLEMENT, ext_csd[EXT_CSD_PSA_ENABLEMENT]);
		printf("MAX Preloading Data Size [PSA_MAX_PRE_LOADING_DATA_SIZE[%u..%u]]: 0x%02x\n",
		       EXT_CSD_PSA_MAX_PRE_LOADING_DATA_SIZE,
		       EXT_CSD_PSA_MAX_PRE_LOADING_DATA_SIZE + 3,
		       get_extcsd_val32(ext_csd, EXT_CSD_PSA_MAX_PRE_LOADING_DATA_SIZE));
		printf("Preloading Data Size [PSA_PRE_LOADING_DATA_SIZE[%u..%u]]: 0x%02x\n",
		       EXT_CSD_PSA_PRE_LOADING_DATA_SIZE,
		       EXT_CSD_PSA_PRE_LOADING_DATA_SIZE + 3,
		       get_extcsd_val32(ext_csd, EXT_CSD_PSA_PRE_LOADING_DATA_SIZE));
		printf("PSA mode [PSA[%u]]: 0x%02x\n", EXT_CSD_PSA, ext_csd[EXT_CSD_PSA]);

		switch (ext_csd[EXT_CSD_PSA]) {
		case 0:
			str = "NORMAL";
			break;

		case 1:
			str = "PRE_SOLDERING_WRITES";
			break;

		case 2:
			str = "PRE_SOLDERING_POST_WRITES";
			break;

		case 3:
			str = "AUTO_PRE_SOLDERING";
			break;

		default:
			str = "undefined";
		}
		printf("  %s\n", str);
	}

	if (ext_csd_rev >= 8) {
		printf("Command Queue Support [CMDQ_SUPPORT]: 0x%02x\n",
		       ext_csd[EXT_CSD_CMDQ_SUPPORT]);
		printf("Command Queue Depth [CMDQ_DEPTH]: %u\n",
		       (ext_csd[EXT_CSD_CMDQ_DEPTH] & 0x1f) + 1);
		printf("Command Enabled [CMDQ_MODE_EN]: 0x%02x\n",
		       ext_csd[EXT_CSD_CMDQ_MODE_EN]);
		printf("Note: CMDQ_MODE_EN may not indicate the runtime CMDQ ON or OFF.\nPlease check sysfs node '/sys/devices/.../mmc_host/mmcX/mmcX:XXXX/cmdq_en'\n");
	}
}

static int do_mmc_extcsd(struct cmd_tbl *cmdtp, int flag,
			 int argc, char *const argv[])
{
	int ret;
	struct mmc *mmc = find_mmc_device(curr_device);
	u8 value;
	u16 index;
	ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);
	char *eol;
	unsigned long val;

	if (argc < 1)
		return CMD_RET_USAGE;

	if (argc > 2) {
		val = simple_strtoul(argv[2], &eol, 0);
		if (argv[2][0] == '\0' || *eol != '\0') {
			printf("Invalid index: '%s'\n", argv[2]);
			return CMD_RET_USAGE;
		}
		index = val;
		if (index >= 512) {
			printf("Index %d is out or range\n", index);
			return CMD_RET_USAGE;
		}
	}

	ret = mmc_send_ext_csd(mmc, ext_csd);
	if (ret) {
		printf("Failed to read ext_csd: %d\n", ret);
		return CMD_RET_FAILURE;
	}
	if (argc == 1) {
		mmc_extcsd_print(ext_csd);
		return CMD_RET_SUCCESS;
	}
	if (strncmp(argv[1], "get", strlen("get")) == 0) {
		int precision;

		if (strcmp(argv[1], "get.32") == 0) {
			val = get_extcsd_val32(ext_csd, index);
			precision = 8;
		} else {
			val = ext_csd[index];
			precision = 2;
		}
		if (argc > 3)
			env_set_ulong(argv[3], val);
		else
			printf("EXT_CSD[%u]=%0*lx\n", index, precision, val);
		return CMD_RET_SUCCESS;
	}
	if (strcmp(argv[1], "set") == 0) {
		if (argc < 4)
			return CMD_RET_USAGE;
		val = simple_strtoul(argv[3], &eol, 0);
		if (argv[3][0] == '\0' || *eol != '\0') {
			printf("Invalid value: '%s'\n", argv[3]);
			return CMD_RET_USAGE;
		}
		value = val;

		ret = mmc_set_extcsd(mmc, index, value);
		if (ret) {
			printf("Failed to set EXT_CSD[%u]=%02x: %d\n",
			       index, value, ret);
			return CMD_RET_FAILURE;
		}
		return CMD_RET_SUCCESS;
	}
	return CMD_RET_USAGE;
}

static int do_mmc_setdsr(struct cmd_tbl *cmdtp, int flag,
			 int argc, char *const argv[])
{
	struct mmc *mmc;
	u32 val;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;
	val = hextoul(argv[1], NULL);

	mmc = find_mmc_device(curr_device);
	if (!mmc) {
		printf("no mmc device at slot %x\n", curr_device);
		return CMD_RET_FAILURE;
	}
	ret = mmc_set_dsr(mmc, val);
	printf("set dsr %s\n", (!ret) ? "OK, force rescan" : "ERROR");
	if (!ret) {
		mmc->has_init = 0;
		if (mmc_init(mmc))
			return CMD_RET_FAILURE;
		else
			return CMD_RET_SUCCESS;
	}
	return ret;
}

#ifdef CONFIG_CMD_BKOPS_ENABLE
static int do_mmc_bkops_enable(struct cmd_tbl *cmdtp, int flag,
			       int argc, char *const argv[])
{
	int dev;
	struct mmc *mmc;

	if (argc != 2)
		return CMD_RET_USAGE;

	dev = dectoul(argv[1], NULL);

	mmc = init_mmc_device(dev, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (IS_SD(mmc)) {
		puts("BKOPS_EN only exists on eMMC\n");
		return CMD_RET_FAILURE;
	}

	return mmc_set_bkops_enable(mmc);
}
#endif

static int do_mmc_boot_wp(struct cmd_tbl *cmdtp, int flag,
			  int argc, char * const argv[])
{
	int err;
	struct mmc *mmc;

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;
	if (IS_SD(mmc)) {
		printf("It is not an eMMC device\n");
		return CMD_RET_FAILURE;
	}
	err = mmc_boot_wp(mmc);
	if (err)
		return CMD_RET_FAILURE;
	printf("boot areas protected\n");
	return CMD_RET_SUCCESS;
}

static struct cmd_tbl cmd_mmc[] = {
	U_BOOT_CMD_MKENT(info, 1, 0, do_mmcinfo, "", ""),
	U_BOOT_CMD_MKENT(read, 4, 1, do_mmc_read, "", ""),
	U_BOOT_CMD_MKENT(wp, 1, 0, do_mmc_boot_wp, "", ""),
#if CONFIG_IS_ENABLED(MMC_WRITE)
	U_BOOT_CMD_MKENT(write, 5, 0, do_mmc_write, "", ""),
	U_BOOT_CMD_MKENT(erase, 3, 0, do_mmc_erase, "", ""),
#endif
#if CONFIG_IS_ENABLED(CMD_MMC_SWRITE)
	U_BOOT_CMD_MKENT(swrite, 3, 0, do_mmc_sparse_write, "", ""),
#endif
	U_BOOT_CMD_MKENT(rescan, 2, 1, do_mmc_rescan, "", ""),
	U_BOOT_CMD_MKENT(part, 1, 1, do_mmc_part, "", ""),
	U_BOOT_CMD_MKENT(dev, 4, 0, do_mmc_dev, "", ""),
	U_BOOT_CMD_MKENT(list, 1, 1, do_mmc_list, "", ""),
#if CONFIG_IS_ENABLED(MMC_HW_PARTITIONING)
	U_BOOT_CMD_MKENT(hwpartition, 28, 0, do_mmc_hwpartition, "", ""),
#endif
#ifdef CONFIG_SUPPORT_EMMC_BOOT
	U_BOOT_CMD_MKENT(bootbus, 5, 0, do_mmc_bootbus, "", ""),
	U_BOOT_CMD_MKENT(bootpart-resize, 4, 0, do_mmc_boot_resize, "", ""),
	U_BOOT_CMD_MKENT(partconf, 5, 0, do_mmc_partconf, "", ""),
	U_BOOT_CMD_MKENT(rst-function, 3, 0, do_mmc_rst_func, "", ""),
#endif
#if CONFIG_IS_ENABLED(CMD_MMC_RPMB)
	U_BOOT_CMD_MKENT(rpmb, CONFIG_SYS_MAXARGS, 1, do_mmcrpmb, "", ""),
#endif
	U_BOOT_CMD_MKENT(setdsr, 2, 0, do_mmc_setdsr, "", ""),
#ifdef CONFIG_CMD_BKOPS_ENABLE
	U_BOOT_CMD_MKENT(bkops-enable, 2, 0, do_mmc_bkops_enable, "", ""),
#endif
	U_BOOT_CMD_MKENT(extcsd, 5, 0, do_mmc_extcsd, "", ""),
};

static int do_mmcops(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	struct cmd_tbl *cp;

	cp = find_cmd_tbl(argv[1], cmd_mmc, ARRAY_SIZE(cmd_mmc));

	/* Drop the mmc command */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;

	if (curr_device < 0) {
		if (get_mmc_num() > 0) {
			curr_device = 0;
		} else {
			puts("No MMC device available\n");
			return CMD_RET_FAILURE;
		}
	}
	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(
	mmc, 29, 1, do_mmcops,
	"MMC sub system",
	"info - display info of the current MMC device\n"
	"mmc read addr blk# cnt\n"
	"mmc write addr blk# cnt\n"
#if CONFIG_IS_ENABLED(CMD_MMC_SWRITE)
	"mmc swrite addr blk#\n"
#endif
	"mmc erase blk# cnt\n"
	"mmc rescan [mode]\n"
	"mmc part - lists available partition on current mmc device\n"
	"mmc dev [dev] [part] [mode] - show or set current mmc device [partition] and set mode\n"
	"  - the required speed mode is passed as the index from the following list\n"
	"    [MMC_LEGACY, MMC_HS, SD_HS, MMC_HS_52, MMC_DDR_52, UHS_SDR12, UHS_SDR25,\n"
	"    UHS_SDR50, UHS_DDR50, UHS_SDR104, MMC_HS_200, MMC_HS_400, MMC_HS_400_ES]\n"
	"mmc list - lists available devices\n"
	"mmc wp - power on write protect boot partitions\n"
#if CONFIG_IS_ENABLED(MMC_HW_PARTITIONING)
	"mmc hwpartition <USER> <GP> <MODE> - does hardware partitioning\n"
	"  arguments (sizes in 512-byte blocks):\n"
	"   USER - <user> <enh> <start> <cnt> <wrrel> <{on|off}>\n"
	"	: sets user data area attributes\n"
	"   GP - <{gp1|gp2|gp3|gp4}> <cnt> <enh> <wrrel> <{on|off}>\n"
	"	: general purpose partition\n"
	"   MODE - <{check|set|complete}>\n"
	"	: mode, complete set partitioning completed\n"
	"  WARNING: Partitioning is a write-once setting once it is set to complete.\n"
	"  Power cycling is required to initialize partitions after set to complete.\n"
#endif
#ifdef CONFIG_SUPPORT_EMMC_BOOT
	"mmc bootbus <dev> <boot_bus_width> <reset_boot_bus_width> <boot_mode>\n"
	" - Set the BOOT_BUS_WIDTH field of the specified device\n"
	"mmc bootpart-resize <dev> <boot part size MB> <RPMB part size MB>\n"
	" - Change sizes of boot and RPMB partitions of specified device\n"
	"mmc partconf <dev> [[varname] | [<boot_ack> <boot_partition> <partition_access>]]\n"
	" - Show or change the bits of the PARTITION_CONFIG field of the specified device\n"
	"   If showing the bits, optionally store the boot_partition field into varname\n"
	"mmc rst-function <dev> <value>\n"
	" - Change the RST_n_FUNCTION field of the specified device\n"
	"   WARNING: This is a write-once field and 0 / 1 / 2 are the only valid values.\n"
#endif
#if CONFIG_IS_ENABLED(CMD_MMC_RPMB)
	"mmc rpmb read addr blk# cnt [address of auth-key] - block size is 256 bytes\n"
	"mmc rpmb write addr blk# cnt <address of auth-key> - block size is 256 bytes\n"
	"mmc rpmb key <address of auth-key> - program the RPMB authentication key.\n"
	"mmc rpmb counter - read the value of the write counter\n"
#endif
	"mmc setdsr <value> - set DSR register value\n"
#ifdef CONFIG_CMD_BKOPS_ENABLE
	"mmc bkops-enable <dev> - enable background operations handshake on device\n"
	"   WARNING: This is a write-once setting.\n"
#endif
	"mmc extcsd - print the contents of the EXT_CSD area\n"
	"mmc extcsd read [addr] - read the contents of the EXT_CSD area to memory\n"
	"                         addr defaults to ${loadaddr}\n"
	"mmc extcsd get index [variable] - read the EXT_CSD value at index\n"
	"                                  and optionally store the value in ${variable}\n"
	"mmc extcsd set index value\n"
	);

/* Old command kept for compatibility. Same as 'mmc info' */
U_BOOT_CMD(
	mmcinfo, 1, 0, do_mmcinfo,
	"display MMC info",
	"- display info of the current MMC device"
);
