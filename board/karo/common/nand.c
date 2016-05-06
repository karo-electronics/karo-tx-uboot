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
#include <nand.h>
#include <mxcfb.h>
#include <linux/list.h>
#include <linux/fb.h>
#include <jffs2/load_kernel.h>
#include <malloc.h>

#include "karo.h"

#ifdef CONFIG_MAX_DTB_SIZE
#define MAX_DTB_SIZE	CONFIG_MAX_DTB_SIZE
#else
#define MAX_DTB_SIZE	SZ_64K
#endif

DECLARE_GLOBAL_DATA_PTR;

int karo_load_nand_part(const char *part, void *addr, size_t len)
{
	int ret;
	struct mtd_device *dev;
	struct part_info *part_info;
	u8 part_num;

	debug("Initializing mtd_parts\n");
	ret = mtdparts_init();
	if (ret)
		return ret;

	debug("Trying to find NAND partition '%s'\n", part);
	ret = find_dev_and_part(part, &dev, &part_num, &part_info);
	if (ret) {
		printf("Failed to find flash partition '%s': %d\n",
			part, ret);

		return ret;
	}
	debug("Found partition '%s': offset=%08x size=%08x\n",
		part, part_info->offset, part_info->size);

	if (part_info->size < len)
		len = part_info->size;

	debug("Reading NAND partition '%s' to %p\n", part, addr);
	ret = nand_read_skip_bad(&nand_info[0], part_info->offset, &len,
				NULL, part_info->size, addr);
	if (ret) {
		printf("Failed to load partition '%s' to %p\n", part, addr);
		return ret;
	}

	debug("Read %u byte from partition '%s' @ offset %08x\n",
		len, part, part_info->offset);
	return 0;
}

#if defined(CONFIG_SPLASH_SCREEN) && defined(CONFIG_MTDPARTS)
static int erase_flash(loff_t offs, size_t len)
{
	nand_erase_options_t nand_erase_options;

	memset(&nand_erase_options, 0, sizeof(nand_erase_options));
	nand_erase_options.length = len;
	nand_erase_options.offset = offs;

	return nand_erase_opts(&nand_info[0], &nand_erase_options);
}

int do_fbdump(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	size_t fbsize = calc_fbsize();
	const char *part = "logo";
	struct mtd_device *dev;
	struct part_info *part_info;
	u8 part_num;
	u_char *addr = (u_char *)gd->fb_base;

	if (argc > 2)
		return CMD_RET_USAGE;

	if (argc == 2)
		part = argv[1];

	if (!addr) {
		printf("fb address unknown\n");
		return CMD_RET_FAILURE;
	}

	debug("Initializing mtd_parts\n");
	ret = mtdparts_init();
	if (ret)
		return CMD_RET_FAILURE;

	debug("Trying to find NAND partition '%s'\n", part);
	ret = find_dev_and_part(part, &dev, &part_num,
				&part_info);
	if (ret) {
		printf("Failed to find flash partition '%s': %d\n",
			part, ret);
		return CMD_RET_FAILURE;
	}
	debug("Found partition '%s': offset=%08x size=%08x\n",
		part, part_info->offset, part_info->size);
	if (part_info->size < fbsize) {
		printf("Error: partition '%s' smaller than frame buffer size: %u\n",
			part, fbsize);
		return CMD_RET_FAILURE;
	}
	debug("Writing framebuffer %p to NAND partition '%s'\n",
		addr, part);

	ret = erase_flash(part_info->offset, fbsize);
	if (ret) {
		printf("Failed to erase partition '%s'\n", part);
		return CMD_RET_FAILURE;
	}

	ret = nand_write_skip_bad(&nand_info[0], part_info->offset,
				&fbsize, NULL, part_info->size,
				addr, WITH_DROP_FFS);
	if (ret) {
		printf("Failed to write partition '%s'\n", part);
		return CMD_RET_FAILURE;
	}

	debug("Wrote %u byte from %p to partition '%s' @ offset %08x\n",
		fbsize, addr, part, part_info->offset);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(fbdump, 2, 0, do_fbdump, "dump framebuffer contents to flash",
	"[partition name]\n"
	"       default partition name: 'logo'\n");
#endif
