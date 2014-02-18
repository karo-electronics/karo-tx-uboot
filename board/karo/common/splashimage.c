/*
 * (C) Copyright 2012 Lothar Waßmann <LW@KARO-electronics.de>
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
#include <lcd.h>
#include <nand.h>
#include <jffs2/load_kernel.h>

DECLARE_GLOBAL_DATA_PTR;

static int karo_load_part(const char *part, void *addr, size_t len)
{
	int ret;
	struct mtd_device *dev;
	struct part_info *part_info;
	u8 part_num;
	size_t actual;

	debug("Initializing mtd_parts\n");
	ret = mtdparts_init();
	if (ret)
		return ret;

	debug("Trying to find NAND partition '%s'\n", part);
	ret = find_dev_and_part(part, &dev, &part_num,
				&part_info);
	if (ret) {
		printf("Failed to find flash partition '%s': %d\n",
			part, ret);

		return ret;
	}
	debug("Found partition '%s': offset=%08x size=%08x\n",
		part, part_info->offset, part_info->size);
	if (part_info->size < len) {
		printf("Warning: partition '%s' smaller than requested size: %u; truncating data to %u byte\n",
			part, len, part_info->size);
		len = part_info->size;
	}
	debug("Reading NAND partition '%s' to %p\n", part, addr);
	ret = nand_read_skip_bad(&nand_info[0], part_info->offset, &len,
				&actual, len, addr);
	if (ret) {
		printf("Failed to load partition '%s' to %p\n", part, addr);
		return ret;
	}
	if (actual < len)
		printf("Read only %u of %u bytes due to bad blocks\n",
			actual, len);

	debug("Read %u byte from partition '%s' @ offset %08x\n",
		len, part, part_info->offset);
	return 0;
}

static ulong calc_fbsize(void)
{
	return panel_info.vl_row * panel_info.vl_col *
		NBITS(panel_info.vl_bpix) / 8;
}

int karo_load_splashimage(int mode)
{
	int ret;
	int do_nand(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);
	unsigned long la = gd->fb_base;
	char *splashimage = getenv("splashimage");
	ulong fbsize = calc_fbsize();
	char *end;

	if (!la || !splashimage)
		return 0;

	if ((simple_strtoul(splashimage, &end, 16) != 0) &&
		*end == '\0') {
		if (mode)
			return 0;
		la = simple_strtoul(splashimage, NULL, 16);
		splashimage = "logo.bmp";
	} else if (!mode) {
		return 0;
	}

	if (had_ctrlc())
		return -ENODEV;

	ret = karo_load_part(splashimage, (void *)la, fbsize);
	if (ret) {
		printf("Failed to load logo from '%s': %d\n", splashimage, ret);
		return ret;
	}
	return 0;
}

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
	size_t actual;

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
		return ret;

	debug("Trying to find NAND partition '%s'\n", part);
	ret = find_dev_and_part(part, &dev, &part_num,
				&part_info);
	if (ret) {
		printf("Failed to find flash partition '%s': %d\n",
			part, ret);

		return ret;
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
				&fbsize, &actual, part_info->size,
				addr, WITH_DROP_FFS);
	if (ret) {
		printf("Failed to write partition '%s'\n", part);
		return ret;
	}
	if (actual < fbsize)
		printf("Wrote only %u of %u bytes due to bad blocks\n",
			actual, fbsize);

	debug("Wrote %u byte from %p to partition '%s' @ offset %08x\n",
		fbsize, addr, part, part_info->offset);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(fbdump, 2, 0, do_fbdump, "dump framebuffer contents to flash",
	"[partition name]\n"
	"       default partition name: 'logo'\n");
