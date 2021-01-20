// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <fdt_support.h>
#include <fs.h>
#include <mmc.h>
#include <mtd_node.h>
#include <asm/bootm.h>
#include <asm/setup.h>
#include "karo.h"

static void karo_set_fdtsize(void *fdt)
{
	size_t fdtsize = env_get_hex("fdt_size", 0);

	if (fdtsize == fdt_totalsize(fdt))
		return;

	debug("FDT size changed from %zu to %u\n",
	      fdtsize, fdt_totalsize(fdt));
	env_set_hex("fdt_size", fdt_totalsize(fdt));
}

int karo_load_fdt(const char *fdt_file)
{
	int ret;
	const char *dev_type = "mmc";
	const char *dev_part = "0";
	ulong fdt_addr = env_get_hex("fdt_addr", _pfx(0x, CONFIG_FDTADDR));
	loff_t size;
	void *fdt;

	printf("loading FDT from %s %s '%s'\n", dev_type, dev_part, fdt_file);

	if (!file_exists(dev_type, dev_part, fdt_file, FS_TYPE_ANY)) {
		printf("'%s' does not exist\n", fdt_file);
		return -ENOENT;
	}
	if (fs_set_blk_dev(dev_type, dev_part, FS_TYPE_ANY))
		return -ENOENT;

	debug("Loading %s to %08lx\n", fdt_file, fdt_addr);
	ret = fs_read(fdt_file, fdt_addr, 0, 0, &size);
	if (ret)
		return ret;
	debug("Read %llu byte from '%s'\n", size, fdt_file);
	fdt = (void *)fdt_addr;
	debug("Checking FDT header @ %p\n", fdt);
	if (fdt_check_header(fdt)) {
		printf("ERROR: No valid DTB found at %p\n", fdt);
		return -EINVAL;
	}
	fdt_shrink_to_minimum(fdt, 4096);
	karo_set_fdtsize(fdt);
	set_working_fdt_addr(fdt_addr);
	debug("fdt_addr set to %08lx\n", fdt_addr);
	return 0;
}

#if defined(CONFIG_FDT_FIXUP_PARTITIONS) && !defined(CONFIG_SPL_BUILD)
void karo_fixup_mtdparts(void *blob, struct node_info *info, size_t count)
{
	int root = fdt_path_offset(blob, "/soc");

	if (root < 0)
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

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	struct tag_serialnr serno;
	char serno_str[64 / 4 + 1];

	get_board_serial(&serno);
	snprintf(serno_str, sizeof(serno_str), "%08x%08x",
		 serno.high, serno.low);

	printf("serial-number: %s\n", serno_str);

	fdt_setprop(blob, 0, "serial-number", serno_str, strlen(serno_str));
	fsl_fdt_fixup_dr_usb(blob, bd);

	fdt_fixup_ethernet(blob);
	return 0;
}
#endif /* OF_BOARD_SETUP */
