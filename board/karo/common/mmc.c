/*
 * (C) Copyright 2014 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <errno.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <mmc.h>
#include <mxcfb.h>
#include <fs.h>
#include <fat.h>
#include <malloc.h>
#include <linux/list.h>
#include <linux/fb.h>
#include <jffs2/load_kernel.h>

#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

#define MAX_SEARCH_PARTITIONS 16

static int find_partitions(const char *ifname, int devno, int fstype,
			block_dev_desc_t **dev_desc, disk_partition_t *info)
{
	int ret = -1;
	char *dup_str = NULL;
	int p;
	int part;
	block_dev_desc_t *dd;

	dd = get_dev(ifname, devno);
	if (!dd || dd->type == DEV_TYPE_UNKNOWN) {
		printf("** Bad device %s %d **\n", ifname, devno);
		return -1;
	}
	init_part(dd);

	/*
	 * No partition table on device,
	 * or user requested partition 0 (entire device).
	 */
	if (dd->part_type == PART_TYPE_UNKNOWN) {
		printf("** No partition table on device %s %d **\n",
			ifname, devno);
		goto cleanup;
	}

	part = 0;
	for (p = 1; p <= MAX_SEARCH_PARTITIONS; p++) {
		ret = get_partition_info(dd, p, info);
		if (ret)
			continue;

		if (fat_register_device(dd, p) == 0) {
			part = p;
			dd->log2blksz = LOG2(dd->blksz);
			break;
		}
	}
	if (!part) {
		printf("** No valid partition on device %s %d **\n",
			ifname, devno);
		ret = -1;
		goto cleanup;
	}

	ret = part;
	*dev_desc = dd;

cleanup:
	free(dup_str);
	return ret;
}

static int karo_mmc_find_part(struct mmc *mmc, const char *part, int devno,
			disk_partition_t *part_info)
{
	int ret;
	block_dev_desc_t *mmc_dev;

	if (strcmp(part, "dtb") == 0) {
		const int partnum = CONFIG_SYS_MMC_ENV_PART;

		part_info->blksz = mmc->read_bl_len;
		part_info->start = CONFIG_SYS_DTB_OFFSET / part_info->blksz;
		part_info->size = CONFIG_SYS_DTB_PART_SIZE / part_info->blksz;
		printf("Using virtual partition %s(%d) ["LBAF".."LBAF"]\n",
			part, partnum, part_info->start,
			part_info->start + part_info->size - 1);
		return partnum;
	}

	ret = find_partitions("mmc", devno, FS_TYPE_FAT, &mmc_dev, part_info);
	if (ret < 0) {
		printf("No eMMC partition found: %d\n", ret);
		return ret;
	}
	return 0;
}

int karo_load_mmc_part(const char *part, void *addr, size_t len)
{
	int ret;
	struct mmc *mmc;
	disk_partition_t part_info;
	int devno = CONFIG_MMC_BOOT_DEV;
	lbaint_t blk_cnt;
	int partnum;

	mmc = find_mmc_device(devno);
	if (!mmc) {
		printf("Failed to find mmc%u\n", devno);
		return -ENODEV;
	}

	if (mmc_init(mmc)) {
		printf("Failed to init MMC device %d\n", devno);
		return -EIO;
	}

	blk_cnt = DIV_ROUND_UP(len, mmc->read_bl_len);

	partnum = karo_mmc_find_part(mmc, part, devno, &part_info);
	if (partnum > 0) {
		if (part_info.start + blk_cnt < part_info.start) {
			printf("%s: given length 0x%08x exceeds size of partition\n",
				__func__, len);
			return -EINVAL;
		}
		if (part_info.start + blk_cnt > mmc->block_dev.lba)
			blk_cnt = mmc->block_dev.lba - part_info.start;

		mmc_switch_part(devno, partnum);

		memset(addr, 0xee, len);

		debug("Reading 0x"LBAF" blks from MMC partition %d offset 0x"LBAF" to %p\n",
			blk_cnt, partnum, part_info.start, addr);
		ret = mmc->block_dev.block_read(devno, part_info.start, blk_cnt, addr);
		if (ret == 0) {
			printf("Failed to read MMC partition %s\n", part);
			ret = -EIO;
			goto out;
		}
		debug("Read %u (%u) byte from partition '%s' @ offset 0x"LBAF"\n",
			ret * mmc->read_bl_len, len, part, part_info.start);
	} else if (partnum == 0) {
		int len_read;

		printf("Reading file %s from mmc partition %d\n", part, 0);
		len_read = fs_read(part, (ulong)addr, 0, len);
		if (len_read < len) {
			printf("Read only %u of %u bytes\n", len_read, len);
		}
	} else {
		ret = partnum;
		goto out;
	}
	ret = 0;
out:
	if (partnum > 0)
		mmc_switch_part(devno, 0);
	return ret < 0 ? ret : 0;
}
