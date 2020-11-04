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
#include <dm/ofnode.h>
#include <dm/of_access.h>
#include <linux/libfdt.h>
#include "karo.h"

#ifdef CONFIG_MAX_DTB_SIZE
#define MAX_DTB_SIZE	CONFIG_MAX_DTB_SIZE
#else
#define MAX_DTB_SIZE	SZ_64K
#endif

DECLARE_GLOBAL_DATA_PTR;
#define FDT_BLOB		((void *)gd->fdt_blob)

static void karo_set_fdtsize(void *fdt)
{
	size_t fdtsize = env_get_ulong("fdtsize", 16, 0);

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

		if (ubi_part(CONFIG_ENV_UBI_PART, NULL)) {
			printf("Cannot find mtd partition '%s'\n",
			       CONFIG_ENV_UBI_PART);
			return NULL;
		}
		ret = ubi_volume_read(dtbfile, fdt, MAX_DTB_SIZE);
		if (ret) {
			printf("Failed to read UBI volume '%s': %d\n",
			       dtbfile, ret);
			return NULL;
		}
		fdtsize = MAX_DTB_SIZE;
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

	debug("%s@%d: fdtaddr=%08lx\n", __func__, __LINE__, fdt_addr);

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
		fdt = FDT_BLOB;
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

	if (of_live_active()) {
		int ret;
		struct device_node *live_fdt;

		ret = of_live_build(fdt, &live_fdt);
		if (ret)
			printf("Failed to build live oftree: %d\n", ret);
		else
			debug("Live oftree built at: %p\n", live_fdt);
	}
	karo_set_fdtsize(fdt);
}

#ifdef CONFIG_DM_VIDEO
static const char * const karo_panel_timing_props[] = {
	"clock-frequency",
	"hactive",
	"vactive",
	"hback-porch",
	"hsync-len",
	"hfront-porch",
	"vback-porch",
	"vsync-len",
	"vfront-porch",
	"hsync-active",
	"vsync-active",
	"de-active",
	"pixelclk-active",
};

static int karo_fixup_panel_timing(ofnode dest, ofnode src)
{
	size_t i;

	printf("Copying video timing from '%s'\n", ofnode_get_name(src));
	for (i = 0; i < ARRAY_SIZE(karo_panel_timing_props); i++) {
		int ret;
		u32 val;
		const char *name = karo_panel_timing_props[i];

		ret = ofnode_read_u32(src, name, &val);
		if (ret) {
			if (ret != FDT_ERR_NOTFOUND) {
				printf("Failed to read property: %s\n",
				       name);
				return ret;
			}
			printf("Removing %s from %s\n",
			       name, ofnode_get_name(dest));
			continue;
		}

		printf("setting %s to <0x%08x>\n", name, fdt32_to_cpu(val));

		ret = ofnode_write_prop(dest, name, sizeof(val),
					(void *)cpu_to_fdt32(val));
		if (ret)
			printf("Failed to set %s property: %s\n", name,
			       fdt_strerror(ret));
	}
	return 0;
}

static int ofnode_create_phandle(ofnode phnode)
{
	int ret;
	uint ph = 0;
	ofnode root = ofnode_path("/");
	ofnode node;
	const struct property *prop;
	int len;

	ofnode_for_each_subnode(node, root) {
		const char *name = "phandle";

		prop = of_find_property(ofnode_to_np(node), name, &len);
		if (!prop) {
			name = "linux,phandle";
			prop = of_find_property(ofnode_to_np(node), name,
						&len);
		}
		if (prop) {
			uint val;

			ret = ofnode_read_u32(node, name, &val);
			if (ret)
				return ret;
			if (fdt32_to_cpu(val) > ph)
				ph = fdt32_to_cpu(val);
		}
	}
	ph++;
	ret = ofnode_write_prop(phnode, "phandle", sizeof(ph),
				(void *)cpu_to_fdt32(ph));
	if (ret)
		return ret;
	return 0;
}

static int karo_fdt_update_native_fb_mode(ofnode node)
{
	int ret;
	u32 ph;

	debug("Creating phandle at node %s\n", ofnode_get_name(node));
	ph = ofnode_create_phandle(node);
	if (!ph) {
		printf("Failed to create phandle for video timing\n");
		return -FDT_ERR_NOSPACE;
	}

	node = ofnode_get_parent(node);
	if (!ofnode_valid(node))
		return -FDT_ERR_NOTFOUND;

	ret = ofnode_write_prop(node, "native-mode", sizeof(ph),
				(void *)cpu_to_fdt32(ph));
	if (ret)
		printf("Failed to set property 'native-mode': %s\n",
		       fdt_strerror(ret));
	karo_set_fdtsize(FDT_BLOB);
	return ret;
}

void karo_fixup_lcd_panel(const char *videomode)
{
	int ret;
	ofnode panel_node = ofnode_path("display");
	ofnode dt_node;
	char *compat = NULL;

	debug("%s@%d: videomode='%s'\n", __func__, __LINE__, videomode);

	if (!ofnode_valid(panel_node)) {
		printf("No 'display' node found\n");
		return;
	}
	if (!videomode) {
		int ret;
		ofnode parent;
		struct ofnode_phandle_args args;

		debug("Disabling LCD panel\n");
		debug("%s@%d: Disabling '%s' node\n", __func__, __LINE__,
		      ofnode_get_name(panel_node));
		ofnode_set_enabled(panel_node, false);
		parent = ofnode_find_subnode(panel_node, "port");
		if (!ofnode_valid(parent))
			return;
		parent = ofnode_find_subnode(parent, "endpoint");
		if (!ofnode_valid(parent))
			return;
		ret = ofnode_parse_phandle_with_args(parent,
						     "remote-endpoint",
						     NULL, 0, 0, &args);
		if (ret)
			return;
		parent = ofnode_get_parent(args.node);
		if (!ofnode_valid(parent))
			return;
		parent = ofnode_get_parent(parent);
		if (!ofnode_valid(parent))
			return;
		debug("%s@%d: Disabling '%s' node\n", __func__, __LINE__,
		      ofnode_get_name(parent));
		ofnode_set_enabled(panel_node, 0);
		return;
	}
	dt_node = ofnode_find_subnode(panel_node, "display-timings");
	if (ofnode_valid(dt_node)) {
		ofnode node = ofnode_find_subnode(dt_node, videomode);
		const char *pn;

		if (!ofnode_valid(node)) {
			printf("Warning: No '%s' subnode found in 'display-timings'\n",
			       videomode);
			return;
		}

		if (ofnode_device_is_compatible(panel_node, "panel-dpi") == 0) {
			ofnode timing_node = ofnode_find_subnode(panel_node,
								 "panel-timing");
			if (!ofnode_valid(timing_node)) {
				printf("Warning: No 'panel-timing' subnode found\n");
				return;
			}
			karo_fixup_panel_timing(timing_node, node);
			return;
		}

		karo_fdt_update_native_fb_mode(node);
		pn = ofnode_read_prop(node, "u-boot,panel-name", NULL);
		if (pn)
			compat = strdup(pn);
		else
			printf("Property 'u-boot,panel-name' not found in '%s'\n",
			       ofnode_get_name(node));
	}
	if (!compat)
		compat = strdup(videomode);

	printf("videomode='%s' compatible='%s'\n", videomode, compat);
	ret = ofnode_write_string(panel_node, "compatible", compat);
	if (ret)
		printf("Warning: Failed to set 'compatible' property for '%s': %s\n",
		       ofnode_get_name(panel_node), fdt_strerror(ret));

	free(compat);
}
#endif

#ifndef CONFIG_SPL_BUILD
void karo_fixup_mtdparts(void *blob, struct node_info *info, size_t count)
{
	ofnode root = ofnode_path("/soc");

	if (!ofnode_valid(root))
		return;

	for (size_t i = 0; i < count; i++) {
		ofnode node = ofnode_by_compatible(root, info[i].compat);

		if (!ofnode_is_available(node))
			continue;
		fdt_fixup_mtdparts(blob, &info[i], 1);
	}
}
#endif
