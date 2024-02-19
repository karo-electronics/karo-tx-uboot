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
#include <asm/cache.h>
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
	int ret = -ENOENT;
	void *fdt = (void *)fdtaddr;
	loff_t fdtsize;
	char *dtbfile;

	/* clear FDT header in memory */
	memset(fdt, 0, 4);

	if (had_ctrlc()) {
		printf("aborting DTB load\n");
		return ERR_PTR(-EINVAL);
	}

#if CONFIG_IS_ENABLED(ENV_IS_IN_MMC)
	if (ret) {
		loff_t fsize;
		const char *bootdev = env_get("bootdev");
		const char *bootpart = env_get("bootpart");

		dtbfile = env_get("dtbfile");

		if (!dtbfile) {
			printf("'dtbfile' not set; cannot load DTB\n");
			return ERR_PTR(-EINVAL);
		}

		printf("Loading DTB from %s %s '%s'\n", bootdev, bootpart,
		       dtbfile);
		ret = fs_set_blk_dev(bootdev, bootpart, FS_TYPE_ANY);
		if (ret)
			return ERR_PTR(ret);

		ret = fs_size(dtbfile, &fsize);
		if (ret) {
			printf("Could not find '%s'\n", dtbfile);
			return ERR_PTR(ret);
		}

		if (fsize > MAX_DTB_SIZE) {
			printf("%s filesize %llu exceeds max. supported DTB size: %llu\n",
			       dtbfile, fsize, MAX_DTB_SIZE);
			return ERR_PTR(-ENOSPC);
		}

		ret = fs_set_blk_dev(bootdev, bootpart, FS_TYPE_ANY);
		if (ret)
			return ERR_PTR(ret);
		ret = fs_read(dtbfile, fdtaddr, 0, MAX_DTB_SIZE, &fdtsize);
		if (ret) {
			printf("Failed to load dtb from '%s': %d\n",
			       dtbfile, ret);
			return ERR_PTR(ret);
		}
	}
#endif
#if CONFIG_IS_ENABLED(ENV_IS_IN_UBI)
	if (ret) {
		dtbfile = "dtb";
		ret = ubi_part(CONFIG_ENV_UBI_PART, CONFIG_ENV_UBI_VID_OFFSET);
		if (ret) {
			printf("Failed to find UBI partition '%s': %d\n",
			       CONFIG_ENV_UBI_PART, ret);
			return ERR_PTR(ret);
		}
		ret = ubi_volume_read(dtbfile, fdt, 0);
		if (ret) {
			printf("Failed to read UBI volume '%s': %d\n",
			       dtbfile, ret);
			return ERR_PTR(ret);
		}
		fdtsize = env_get_hex("filesize", 0);
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

static const char *karo_fdt_overlay_filename(const char *soc_prefix,
					     const char *baseboard)
{
	size_t malloc_size = strlen(soc_prefix) + strlen(baseboard) + 6;
	char *fdtfile = malloc(malloc_size);
	const char *pfx_end;
	size_t pos;

	if (!fdtfile)
		return NULL;

	strcpy(fdtfile, soc_prefix);
	pfx_end = strchrnul(fdtfile, '-');
	pos = pfx_end - fdtfile;
	snprintf(&fdtfile[pos], malloc_size - pos, "%s%s.dtb",
		 *pfx_end == '-' ? "" : "-", baseboard);

	return fdtfile;
}

int karo_load_fdt_overlay(void *fdt,
			  const char *dev_type, const char *dev_part,
			  const char *soc_prefix, const char *baseboard)
{
	int ret;
	loff_t size;
	loff_t read_size;
	void *fdto;
	const char *filename = karo_fdt_overlay_filename(soc_prefix, baseboard);

	if (!filename)
		return -ENOMEM;

	printf("Loading FDT overlay for '%s' from %s %s '%s'\n", baseboard,
	       dev_type, dev_part, filename);

	if (!file_exists(dev_type, dev_part, filename, FS_TYPE_ANY)) {
		printf("'%s' does not exist\n", filename);
		ret = -ENOENT;
		goto free_fn;
	}
	if (fs_set_blk_dev(dev_type, dev_part, FS_TYPE_ANY)) {
		ret = -ENOENT;
		goto free_fn;
	}

	ret = fs_size(filename, &size);
	if (ret)
		goto free_fn;

	fdto = memalign(ARCH_DMA_MINALIGN, size);
	if (!fdto) {
		printf("%s@%d: failed to allocate %llu bytes for '%s'\n",
		       __func__, __LINE__, size, filename);
		ret = -ENOMEM;
		goto free_fn;
	}

	debug("%s@%d: reading %llu bytes from '%s' to %p\n", __func__, __LINE__,
	      size, filename, fdto);

	if (fs_set_blk_dev(dev_type, dev_part, FS_TYPE_ANY)) {
		ret = -ENOENT;
		goto free_buf;
	}

	ret = fs_read(filename, (ulong)fdto, 0, 0, &read_size);
	if (ret)
		goto free_buf;

	if (read_size != size) {
		printf("Read only %llu bytes of %llu from %s\n",
		       read_size, size, filename);
		goto free_buf;
	}
	debug("Read %llu byte from '%s'\n", size, filename);
	fdt_shrink_to_minimum(fdt, size);

	ret = fdt_overlay_apply_verbose(fdt, fdto);
	if (ret) {
		printf("Failed to load FDT overlay from '%s': %s\n",
		       filename, fdt_strerror(ret));
		ret = -EINVAL;
	}

 free_buf:
	free(fdto);

 free_fn:
	free((void *)filename);
	return ret;
}

int karo_fdt_get_overlays(const char *baseboard, char **overlays)
{
	int ret;

	if (baseboard) {
		const char *prefix = "overlays_";
		size_t malloc_size = strlen(prefix) + strlen(baseboard) + 1;
		char *overlay_var = malloc(malloc_size);

		if (!overlay_var)
			return -ENOMEM;
		ret = snprintf(overlay_var, malloc_size, "%s%s",
			       prefix, baseboard);
		if (ret < 0 || ret >= malloc_size) {
			free(overlay_var);
			return ret < 0 ? ret : -ENOMEM;
		}
		*overlays = env_get(overlay_var);
		free(overlay_var);
		if (*overlays) {
			if (strstr(*overlays, "setenv overlays"))
				*overlays += strlen("setenv overlays ");
		}
	} else {
		*overlays = env_get("overlays");
	}
	return 0;
}

void karo_fdt_apply_overlays(unsigned long fdt_addr)
{
	int ret;
	const char *baseboard = env_get("baseboard");
	const char *dev_type = "mmc";
	const char *dev_part = env_get("bootpart");
	const char *soc_prefix = env_get("soc_prefix");
	char *overlays;

	ret = karo_fdt_get_overlays(baseboard, &overlays);
	if (ret == 0 && overlays) {
		char *overlay_list = strdup(overlays);
		const char *overlay_listp = overlay_list;
		char *overlay;

		debug("Loading FDT overlays for '%s': %s\n", baseboard, overlays);
		while ((overlay = strsep(&overlay_list, ", "))) {
			if (!strlen(overlay))
				continue;
			ret = karo_load_fdt_overlay((void *)fdt_addr, dev_type,
						    dev_part, soc_prefix, overlay);
			if (ret) {
				printf("Failed to load '%s' FDT overlay: %d\n",
				       overlay, ret);
				break;
			}
		}
		free((void *)overlay_listp);
	} else if (ret) {
		printf("Failed to load FDT overlays: %d\n", ret);
	} else {
		printf("No FDT overlays to be loaded\n");
	}
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
	if (IS_ERR(fdt))
		return;
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
	karo_fdt_apply_overlays(fdt_addr);
	fdt_shrink_to_minimum(fdt, 4096);
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
		fdt_set_node_status(fdt, panel_node, FDT_STATUS_DISABLED);
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
		fdt_set_node_status(fdt, panel_node, FDT_STATUS_DISABLED);
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

#ifndef CONFIG_SPL_BUILD
void karo_fixup_mtdparts(void *blob, struct node_info *info, size_t count)
{
	int root = fdt_path_offset(blob, "/soc");

	if (root <= 0)
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
