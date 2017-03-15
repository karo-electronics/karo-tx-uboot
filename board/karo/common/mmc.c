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
#include <blk.h>
#include <errno.h>
#include <fat.h>
#include <fdt_support.h>
#include <fs.h>
#include <libfdt.h>
#include <mmc.h>
#include <malloc.h>
#include <part.h>
#include <linux/err.h>
#include <jffs2/load_kernel.h>

#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

#define MAX_SEARCH_PARTITIONS 16

static int find_partitions(const char *ifname, int devno, const char *partname,
			struct blk_desc **dev_desc, disk_partition_t *info)
{
	struct blk_desc *dd;
	char dev_part_str[16];
	int p;

	dd = blk_get_devnum_by_typename(ifname, devno);
	if (!dd || dd->type == DEV_TYPE_UNKNOWN) {
		printf("** Bad device %s %d **\n", ifname, devno);
		return -1;
	}
	part_init(dd);

	/*
	 * No partition table on device,
	 * or user requested partition 0 (entire device).
	 */
	if (dd->part_type == PART_TYPE_UNKNOWN) {
		printf("** No partition table on device %s %d **\n",
			ifname, devno);
		return -ENODEV;
	}

	printf("part type: %08x\n", dd->part_type);
	for (p = 1; p <= MAX_SEARCH_PARTITIONS; p++) {
		int ret;

		if (partname) {
			ret = part_get_info(dd, p, info);
			if (ret == 0)
				ret = strncmp((char *)info->name, partname,
					sizeof(info->name));
		} else {
			ret = fat_register_device(dd, p);

		}
		if (ret)
			continue;

		snprintf(dev_part_str, sizeof(dev_part_str), "%d:%d",
			devno, p);
		ret = fs_set_blk_dev(ifname, dev_part_str, FS_TYPE_ANY);
		if (ret == 0) {
			dd->log2blksz = LOG2(dd->blksz);
			*dev_desc = dd;
			return 0;
		}
	}
	printf("** No valid partition on device %s %d **\n",
		ifname, devno);
	return -ENODEV;
}

static int karo_mmc_find_part(struct mmc *mmc, const char *part,
			const char *filename, int devno,
			disk_partition_t *part_info)
{
	int ret;
	struct blk_desc *mmc_dev;

#if defined(CONFIG_SYS_DTB_OFFSET) && defined(CONFIG_SYS_MMC_ENV_PART)
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
#endif
	ret = find_partitions("mmc", devno, part, &mmc_dev, part_info);
	if (ret < 0) {
		printf("No (e)MMC partition found: %d\n", ret);
		return ret;
	}
	return 0;
}

int karo_mmc_load_part(const char *part_file, void *addr, size_t len)
{
	int ret;
	struct mmc *mmc;
	char *partname = strdup(part_file);
	char *filename;
	disk_partition_t part_info;
	int devno = mmc_get_env_dev();
	lbaint_t blk_cnt;

	if (!partname)
		return -ENOMEM;

	mmc = find_mmc_device(devno);
	if (!mmc) {
		printf("Failed to find mmc%u\n", devno);
		return -ENODEV;
	}

	if (mmc_init(mmc)) {
		printf("Failed to init MMC device %d\n", devno);
		return -EIO;
	}

	filename = strchr(partname, ':');
	if (filename) {
		*filename = '\0';
		filename++;
	} else {
		filename = partname;
	}

	blk_cnt = DIV_ROUND_UP(len, mmc->read_bl_len);

	ret = karo_mmc_find_part(mmc, partname, filename, devno, &part_info);
	if (ret < 0)
		goto out;

	if (partname == filename) {
		int partnum = ret;
		struct blk_desc *desc = mmc_get_blk_desc(mmc);
		int hwpart = desc->hwpart;
		ulong retval;

		if (part_info.start + blk_cnt < part_info.start) {
			printf("%s: given length 0x%08zx exceeds size of partition\n",
				__func__, len);
			ret = -EINVAL;
			goto out;
		}
		if (partnum != hwpart) {
			ret = blk_select_hwpart_devnum(IF_TYPE_MMC, devno,
						partnum);
			if (ret)
				goto out;

			desc = mmc_get_blk_desc(mmc);
		}
		if (part_info.start + blk_cnt > desc->lba)
			blk_cnt = desc->lba - part_info.start;

		debug("Reading 0x"LBAF" blks from MMC partition %d offset 0x"LBAF" to %p\n",
			blk_cnt, partnum, part_info.start, addr);

		retval = blk_dread(desc, part_info.start, blk_cnt, addr);
		if (partnum != hwpart)
			blk_select_hwpart_devnum(IF_TYPE_MMC, devno, hwpart);
		if (IS_ERR_VALUE(retval) || retval == 0) {
			printf("Failed to read file %s from MMC partition %s\n",
				filename, partname);
			ret = retval ?: -EIO;
			goto out;
		}
		debug("Read %lu (%zu) byte from partition '%s' @ offset 0x"LBAF"\n",
			retval * mmc->read_bl_len, len, filename, part_info.start);
	} else {
		loff_t len_read;

		debug("Trying to read (%zu) byte from file '%s' in mmc partition '%s'\n",
			len, filename, partname);
		ret = fs_read(filename, (ulong)addr, 0, len, &len_read);
		if (ret < 0) {
			printf("Failed to read %zu byte from '%s' in mmc partition '%s'; err: %d\n",
				len, filename, partname, ret);
			goto out;
		}
		debug("Read %llu bytes from %s\n", len_read, filename);
	}
out:
	free(partname);
	return ret;
}
