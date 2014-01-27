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
//#include <nand.h>
#include <mmc.h>
#include <mxcfb.h>
#include <linux/list.h>
#include <linux/fb.h>
#include <jffs2/load_kernel.h>
#include <malloc.h>

#include "karo.h"

#define CONFIG_MMC_BOOT_DEV 0

DECLARE_GLOBAL_DATA_PTR;

static void __maybe_unused memdmp(void *addr, size_t len)
{
	size_t i;

	for (i = 0; i < len; i+= 16) {
		size_t j;
		u32 *wp = addr + i;

		debug("%p: ", addr + i);
		for (j = 0; j < 4; j++) {
			debug(" %08x", wp[j]);
		}
		debug("\n");
	}
}

#define MAX_SEARCH_PARTITIONS 16

static int find_efi_partition(const char *ifname, int devno, const char *part_str,
			     block_dev_desc_t **dev_desc,
			     disk_partition_t *info)
{
	int ret = -1;
	char *dup_str = NULL;
	int p;
	int part;
	block_dev_desc_t *dd;

printf("Searching for partition '%s'\n", part_str);

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
		printf("** No partition table - %s %d **\n", ifname,
			devno);
		goto cleanup;
	}

	part = 0;
	for (p = 1; p <= MAX_SEARCH_PARTITIONS; p++) {
		ret = get_partition_info(dd, p, info);
		if (ret)
			continue;

		if (strcmp((char *)info->name, part_str) == 0) {
			part = p;
			dd->log2blksz = LOG2(dd->blksz);
			break;
		}
	}
	if (!part) {
		printf("** No valid partitions found **\n");
		ret = -1;
		goto cleanup;
	}

	ret = part;
	*dev_desc = dd;

cleanup:
	free(dup_str);
	return ret;
}

int karo_load_mmc_part(const char *part, void *addr, size_t len)
{
	int ret;
	struct mmc *mmc;
	disk_partition_t part_info;
	int devno = CONFIG_MMC_BOOT_DEV;
	uint blk_start, blk_cnt;

	mmc = find_mmc_device(devno);
	if (!mmc) {
		printf("Failed to find mmc%u\n", devno);
		return -ENODEV;
	}

	mmc_init(mmc);

//	mmc_boot_part_access(mmc, 1, part_num, part_num);
#if 1
	block_dev_desc_t *mmc_dev;

	ret = find_efi_partition("mmc", devno, part, &mmc_dev, &part_info);
	if (ret < 0) {
		printf("eMMC partition '%s' not found: %d\n", part, ret);
		goto out;
	}
	mmc_switch_part(devno, ret);

	blk_start = 0;
	blk_cnt = DIV_ROUND_UP(len, part_info.blksz);

	printf("Using mmc%d blksz %lu blks %lu\n", devno,
		mmc_dev->blksz, mmc_dev->lba);
#endif
	debug("Found partition '%s': offset=%08x size=%08lx\n",
		part, blk_start, part_info.size);
	if (part_info.size < blk_cnt) {
		printf("Warning: partition '%s' smaller than requested size: %u; truncating data to %lu blocks\n",
			part, len, part_info.size * mmc->read_bl_len);
		blk_cnt = part_info.size;
	}

	debug("Reading %u blks from MMC partition '%s' offset %u to %p\n",
		blk_cnt, part, blk_start, addr);
	ret = mmc->block_dev.block_read(devno, blk_start, blk_cnt, addr);
	if (ret == 0) {
		printf("Failed to read MMC partition %s\n", part);
		goto out;
	}
	debug("Read %u byte from partition '%s' @ offset %08x\n",
		ret * mmc->read_bl_len, part, blk_start);
	memdmp(addr, 512);
	ret = 0;
out:
//	mmc_boot_part_access(mmc, 1, part_num, 0);
	mmc_switch_part(devno, 0);
	return ret;
}
