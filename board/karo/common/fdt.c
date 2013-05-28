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
#include <libfdt.h>
#include <fdt_support.h>
#include <nand.h>
#include <jffs2/load_kernel.h>

#include "karo.h"

#ifdef CONFIG_MAX_DTB_SIZE
#define MAX_DTB_SIZE	CONFIG_MAX_DTB_SIZE
#else
#define MAX_DTB_SIZE	SZ_64K
#endif

DECLARE_GLOBAL_DATA_PTR;

static void karo_set_fdtsize(void *fdt)
{
	char fdt_size[9];
	size_t fdtsize = getenv_ulong("fdtsize", 16, 0);

	if (fdtsize == fdt_totalsize(fdt)) {
		return;
	}
	debug("FDT size changed from %u to %u\n",
		fdtsize, fdt_totalsize(fdt));

	snprintf(fdt_size, sizeof(fdt_size), "%08x", fdt_totalsize(fdt));
	setenv("fdtsize", fdt_size);
}

void karo_fdt_move_fdt(void)
{
	void *fdt;
	unsigned long fdt_addr = getenv_ulong("fdtaddr", 16, 0);

	if (!fdt_addr) {
		fdt_addr = CONFIG_SYS_FDT_ADDR;
		printf("fdtaddr is not set; using default: %08lx\n",
			fdt_addr);
	}

	fdt = karo_fdt_load_dtb();
	if (fdt == NULL) {
		fdt = (void *)gd->fdt_blob;
		if (fdt == NULL) {
			printf("Compiled in FDT not found\n");
			return;
		}
		debug("Checking FDT header @ %p\n", fdt);
		if (fdt_check_header(fdt)) {
			printf("ERROR: No valid DTB found at %p\n", fdt);
			return;
		}
		printf("No DTB in flash; using default DTB\n");
		debug("Moving FDT from %p..%p to %08lx..%08lx\n",
			fdt, fdt + fdt_totalsize(fdt) - 1,
			fdt_addr, fdt_addr + fdt_totalsize(fdt) - 1);
		memmove((void *)fdt_addr, fdt, fdt_totalsize(fdt));
	}
	set_working_fdt_addr((void *)fdt_addr);
	karo_set_fdtsize(fdt);
}

void karo_fdt_remove_node(void *blob, const char *node)
{
	debug("Removing node '%s' from DT\n", node);
	int off = fdt_path_offset(blob, node);
	int ret;

	if (off < 0) {
		printf("Could not find node '%s': %d\n", node, off);
	} else {
		ret = fdt_del_node(blob, off);
		if (ret)
			printf("Failed to remove node '%s': %d\n",
				node, ret);
	}
	karo_set_fdtsize(blob);
}

void karo_fdt_enable_node(void *blob, const char *node, int enable)
{
	int off = fdt_path_offset(blob, node);

	debug("%sabling node '%s'\n", enable ? "En" : "Dis", node);
	if (off < 0) {
		printf("Could not find node '%s': %d\n", node, off);
		return;
	}
	fdt_set_node_status(blob, off,
			enable ? FDT_STATUS_OKAY : FDT_STATUS_DISABLED, 0);

	karo_set_fdtsize(blob);
}

static const char *karo_touchpanels[] = {
	"ti,tsc2007",
	"edt,edt-ft5x06",
};

static void fdt_del_tp_node(void *blob, const char *name)
{
	int offs = fdt_node_offset_by_compatible(blob, -1, name);
	uint32_t ph1 = 0, ph2 = 0;
	const uint32_t *prop;

	if (offs < 0) {
		debug("node '%s' not found: %d\n", name, offs);
		return;
	}

	prop = fdt_getprop(blob, offs, "reset-switch", NULL);
	if (prop)
		ph1 = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, offs, "wake-switch", NULL);
	if (prop)
		ph2 = fdt32_to_cpu(*prop);

	debug("Removing node '%s' from DT\n", name);
	fdt_del_node(blob, offs);

	if (ph1) {
		offs = fdt_node_offset_by_phandle(blob, ph1);
		if (offs > 0) {
			debug("Removing node %08x @ %08x\n", ph1, offs);
			fdt_del_node(blob, offs);
		}
	}
	if (ph2) {
		offs = fdt_node_offset_by_phandle(blob, ph2);
		if (offs > 0) {
			debug("Removing node %08x @ %08x\n", ph2, offs);
			fdt_del_node(blob, offs);
		}
	}
}

void karo_fdt_fixup_touchpanel(void *blob)
{
	int i;
	const char *model = getenv("touchpanel");

	for (i = 0; i < ARRAY_SIZE(karo_touchpanels); i++) {
		const char *tp = karo_touchpanels[i];

		if (model != NULL && strcmp(model, tp) == 0)
			continue;

		if (model != NULL) {
			if (strcmp(model, tp) == 0)
				continue;
			tp = strchr(tp, ',');
			if (tp != NULL && *tp != '\0' && strcmp(model, tp + 1) == 0)
				continue;
		}
		fdt_del_tp_node(blob, karo_touchpanels[i]);
		karo_set_fdtsize(blob);
	}
}

void karo_fdt_fixup_usb_otg(void *blob, const char *compat, phys_addr_t offs)
{
	const char *otg_mode = getenv("otg_mode");
	int off;

	debug("OTG mode is '%s'\n", otg_mode ? otg_mode : "<UNSET>");

	off = fdt_node_offset_by_compat_reg(blob, compat, offs);
	if (off < 0) {
		printf("Failed to find node %s@%08lx\n", compat, offs);
		return;
	}

	if (!otg_mode || strcmp(otg_mode, "device") != 0) {
		printf("Deleting property gadget-device-name from node %s@%08lx\n",
			compat, offs);
		fdt_delprop(blob, off, "gadget-device-name");
	}
	if (!otg_mode || strcmp(otg_mode, "host") != 0) {
		printf("Deleting property host-device-name from node %s@%08lx\n",
			compat, offs);
		fdt_delprop(blob, off, "host-device-name");
	}
	karo_set_fdtsize(blob);
}

void karo_fdt_del_prop(void *blob, const char *compat, phys_addr_t offs,
			const char *prop)
{
	int ret;
	int offset;
	const uint32_t *phandle;
	uint32_t ph = 0;

	offset = fdt_node_offset_by_compat_reg(blob, compat, offs);
	if (offset <= 0)
		return;

	phandle = fdt_getprop(blob, offset, prop, NULL);
	if (phandle) {
		ph = fdt32_to_cpu(*phandle);
	}

	debug("Removing property '%s' from node %s@%08lx\n", prop, compat, offs);
	ret = fdt_delprop(blob, offset, prop);
	if (ret == 0)
		karo_set_fdtsize(blob);

	if (!ph)
		return;

	offset = fdt_node_offset_by_phandle(blob, ph);
	if (offset <= 0)
		return;

	debug("Removing node @ %08x\n", offset);
	fdt_del_node(blob, offset);
	karo_set_fdtsize(blob);
}

static int karo_load_part(const char *part, void *addr, size_t len)
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
	ret = nand_read_skip_bad(&nand_info[0], part_info->offset, &len, addr);
	if (ret) {
		printf("Failed to load partition '%s' to %p\n", part, addr);
		return ret;
	}
	debug("Read %u byte from partition '%s' @ offset %08x\n",
		len, part, part_info->offset);
	return 0;
}

void *karo_fdt_load_dtb(void)
{
	int ret;
	void *fdt = (void *)getenv_ulong("fdtaddr", 16, 0);

	if (tstc() || !fdt) {
		debug("aborting DTB load\n");
		return NULL;
	}

	/* clear FDT header in memory */
	memset(fdt, 0, 4);

	ret = karo_load_part("dtb", fdt, MAX_DTB_SIZE);
	if (ret) {
		printf("Failed to load dtb from flash: %d\n", ret);
		return NULL;
	}

	if (fdt_check_header(fdt)) {
		debug("No valid DTB in flash\n");
		return NULL;
	}
	debug("Using DTB from flash\n");
	karo_set_fdtsize(fdt);
	return fdt;
}
