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
	char *fdt_file;

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

		fdt_file = env_get("fdt_file");
		debug("%s@%d:\n", __func__, __LINE__);

		if (!fdt_file) {
			debug("'fdt_file' not set; cannot load DTB\n");
			return NULL;
		}

		if (fs_set_blk_dev(bootdev, bootpart, FS_TYPE_ANY))
			return NULL;

		ret = fs_read(fdt_file, fdtaddr, 0, MAX_DTB_SIZE, &fdtsize);
		if (ret) {
			printf("Failed to load dtb from '%s': %d\n",
			       fdt_file, ret);
			return NULL;
		}
	}
#endif
#if CONFIG_IS_ENABLED(ENV_IS_IN_UBI)
	{
		fdt_file = "dtb";

		ret = ubi_volume_read(fdt_file, fdt, 0);
		if (ret) {
			printf("Failed to read UBI volume '%s': %d\n",
			       fdt_file, ret);
			return NULL;
		}
		fdtsize = env_get_hex("filesize", 0);
	}
#endif
	debug("FDT loaded from %s (%llu bytes)\n", fdt_file, fdtsize);
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

static int karo_fixup_panel_timing(void *fdt, int dest, int src)
{
	size_t i;

	printf("Copying video timing from '%s'\n", fdt_get_name(fdt, src, NULL));
	for (i = 0; i < ARRAY_SIZE(karo_panel_timing_props); i++) {
		int ret;
		int len;
		const void *prop;
		const char *name = karo_panel_timing_props[i];
		const void *old;

		prop = fdt_getprop(fdt, src, name, &len);

		if (!prop) {
			debug("Removing %s from %s\n",
			      name, fdt_get_name(fdt, dest, NULL));
			fdt_delprop(fdt, dest, name);
			continue;
		}

		old = fdt_getprop(fdt, dest, name, NULL);
		if (prop && old == prop) {
			debug("leaving %s at <%u>\n", name, be32_to_cpup(old));
		} else if (old && prop && old != prop) {
			debug("changing %s from <%u> to <%u>\n", name,
			      be32_to_cpup(old), be32_to_cpup(prop));
		} else if (!old) {
			debug("adding %s = <%u>\n", name,
			      be32_to_cpup(prop));
			old = prop;
			ret = fdt_setprop(fdt, dest, name, old, len);
			return ret < 0 ? ret : 1;
		} else if (!prop) {
			debug("Removing %s from %s\n",
			      name, fdt_get_name(fdt, dest, NULL));
			fdt_delprop(fdt, dest, name);
			return 1;
		} else {
			debug("old=%p prop=%p\n", old, prop);
			continue;
		}
		ret = fdt_setprop_inplace(fdt, dest, name, prop, len);
		if (ret)
			printf("Failed to set %s property: %s\n", name,
			       fdt_strerror(ret));
	}
	return 0;
}

void karo_fixup_lcd_panel(const char *videomode)
{
	int ret;
	void *fdt = working_fdt;
	int panel_node = fdt_path_offset(fdt, "display");
	int dt_node;
	const char *compat = NULL;

	if (!fdt) {
		printf("No FDT loaded\n");
		return;
	}

	if (panel_node <= 0) {
		printf("No 'display' node found\n");
		return;
	}
	if (!videomode) {
		int ret;
		int parent;
		int pparent;
		struct fdtdec_phandle_args args;

		debug("Disabling LCD panel\n");
		debug("%s@%d: Disabling '%s' node\n", __func__, __LINE__,
		      fdt_get_name(fdt, panel_node, NULL));
		fdt_set_node_status(fdt, panel_node, FDT_STATUS_DISABLED, 0);
		parent = fdt_subnode_offset(fdt, panel_node, "port");
		if (parent <= 0) {
			debug("%s@%d: No 'port' node found in '%s'\n",
			      __func__, __LINE__, fdt_get_name(fdt, panel_node, NULL));
			return;
		}
		parent = fdt_subnode_offset(fdt, parent, "endpoint");
		if (parent <= 0) {
			debug("%s@%d: No 'endpoint' node found in '%s'\n",
			      __func__, __LINE__, fdt_get_name(fdt, parent, NULL));
			return;
		}
		ret = fdtdec_parse_phandle_with_args(fdt, parent,
						     "remote-endpoint",
						     NULL, 0, 0, &args);
		if (ret) {
			debug("%s@%d: Could not parse 'remote-endpoint' in '%s'\n",
			      __func__, __LINE__, fdt_get_name(fdt, parent, NULL));
			return;
		}
		parent = fdt_parent_offset(fdt, args.node);
		if (parent <= 0) {
			debug("%s@%d: No parent found for '%s'\n",
			      __func__, __LINE__, fdt_get_name(fdt, args.node, NULL));
			return;
		}

		pparent = fdt_parent_offset(fdt, parent);
		if (pparent <= 0) {
			debug("%s@%d: No parent found for '%s'\n",
			      __func__, __LINE__, fdt_get_name(fdt, parent, NULL));
			return;
		}
		debug("%s@%d: Disabling '%s' node\n", __func__, __LINE__,
		      fdt_get_name(fdt, pparent, NULL));
		fdt_set_node_status(fdt, panel_node, FDT_STATUS_DISABLED, 0);
		return;
	}
	dt_node = fdt_subnode_offset(fdt, panel_node, "display-timings");
	if (dt_node > 0) {
		int node = fdt_subnode_offset(fdt, dt_node, videomode);
		const char *pn;

		if (node <= 0) {
			printf("Warning: No '%s' subnode found in 'display-timings'\n",
			       videomode);
			return;
		}

		if (fdt_node_check_compatible(fdt, panel_node, "panel-dpi") == 0) {
			int timing_node = fdt_subnode_offset(fdt, panel_node,
							     "panel-timing");

			if (timing_node <= 0) {
				printf("Warning: No 'panel-timing' subnode found in '%s'\n",
				       fdt_get_name(fdt, panel_node, NULL));
				return;
			}
			while (karo_fixup_panel_timing(fdt, timing_node, node) == 1) {
				panel_node = fdt_path_offset(fdt, "display");
				dt_node = fdt_subnode_offset(fdt, panel_node, "display-timings");
				node = fdt_subnode_offset(fdt, dt_node, videomode);
				timing_node = fdt_subnode_offset(fdt, panel_node,
								 "panel-timing");
			}
			return;
		}

		pn = fdt_getprop(fdt, node, "u-boot,panel-name", NULL);
		debug("%s@%d: panel-name='%s'\n", __func__, __LINE__,
		      pn ?: "<N/A>");
		if (pn)
			compat = pn;
		else
			printf("Property 'u-boot,panel-name' not found in '%s'\n",
			       fdt_get_name(fdt, node, NULL));
	}
	if (!compat)
		compat = videomode;

	ret = fdt_setprop_string(fdt, panel_node, "compatible", compat);
	if (ret)
		printf("Warning: Failed to set 'compatible' property for '%s': %s\n",
		       fdt_get_name(fdt, panel_node, NULL), fdt_strerror(ret));
}
#endif
