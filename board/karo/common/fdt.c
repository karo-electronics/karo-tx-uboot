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
#include <malloc.h>
#include <asm/bootm.h>
#include <asm/cache.h>
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

#ifdef CONFIG_TX8M_UBOOT
static char *karo_fdt_overlay_filename(const char *prefix,
				       const char *overlay)
{
	size_t malloc_size = strlen(prefix) + strlen(overlay) + 6;
	char *fdtfile = malloc(malloc_size);
	const char *pfx_end;
	size_t pos;

	if (!fdtfile)
		return NULL;

	strcpy(fdtfile, prefix);
	pfx_end = strchrnul(fdtfile, '-');
	pos = pfx_end - fdtfile;
	snprintf(&fdtfile[pos], malloc_size - pos, "%s%s.dtb",
		 *pfx_end == '-' ? "" : "-", overlay);

	return fdtfile;
}

static bool fdt_overlay_debug = IS_ENABLED(DEBUG);

int karo_load_fdt_overlay(void *fdt, const char *dev_type, const char *dev_part,
			  const char *overlay)
{
	int ret;
	loff_t size;
	void *fdto;
	const char *soc_prefix = env_get("soc_prefix");
	const char *soc_family = env_get("soc_family");
	char *filename = karo_fdt_overlay_filename(soc_family, overlay);

	if (!filename)
		return -ENOMEM;

	if (!file_exists(dev_type, dev_part, filename, FS_TYPE_ANY)) {
		free(filename);
		filename = karo_fdt_overlay_filename(soc_prefix, overlay);
		if (!filename)
			return -ENOMEM;
	}
	if (fdt_overlay_debug)
		printf("loading FDT overlay for '%s' from %s %s '%s'\n",
		       overlay, dev_type, dev_part, filename);

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
	if (ret) {
		printf("Failed to get size of '%s': %d\n", filename, errno);
		goto free_fn;
	}

	fdto = memalign(ARCH_DMA_MINALIGN, size);
	if (!fdto) {
		printf("%s@%d: failed to allocate %llu bytes for '%s'\n",
		       __func__, __LINE__, size, filename);
		ret = -ENOMEM;
		goto free_fn;
	}

	debug("%s@%d: reading %llu bytes from '%s'\n", __func__, __LINE__,
	      size, filename);

	if (fs_set_blk_dev(dev_type, dev_part, FS_TYPE_ANY)) {
		ret = -ENOENT;
		goto free_buf;
	}

	ret = fs_read(filename, (ulong)fdto, 0, 0, &size);
	if (ret)
		goto free_buf;

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
		if (strstr(*overlays, "setenv overlays"))
			*overlays += strlen("setenv overlays ");
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
	const char *dev_part = "0:1";
	char *overlays;

	fdt_overlay_debug |= env_get_yesno("debug_overlays") == 1;

	ret = karo_fdt_get_overlays(baseboard, &overlays);
	if (ret == 0 && overlays) {
		char *overlay_list = strdup(overlays);
		const char *overlay_listp = overlay_list;
		char *overlay;

		debug("loading FDT overlays for '%s': %s\n",
		      baseboard, overlays);
		while ((overlay = strsep(&overlay_list, ", "))) {
			if (!strlen(overlay))
				continue;
			ret = karo_load_fdt_overlay((void *)fdt_addr, dev_type,
						    dev_part, overlay);
			if (ret) {
				printf("Failed to load FDT overlay '%s': %d\n",
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
	if (ret)
		memset((void *)fdt_addr, 0, sizeof(fdt_addr));
}
#else
static inline void karo_fdt_apply_overlays(unsigned long fdt_addr)
{
}
#endif

int karo_load_fdt(const char *fdt_file)
{
	int ret;
	const char *dev_type = "mmc";
	const char *envstr = env_get("mmcdev");
	char *eos;
	int mmcdev = 0;
	int mmcpart = 1;
	char dev_part[8];
	ulong fdt_addr = env_get_hex("fdt_addr", _pfx(0x, CONFIG_FDTADDR));
	loff_t size;
	void *fdt;

	if (envstr) {
		mmcdev = simple_strtoul(envstr, &eos, 10);
		if (*eos != '\0' || mmcdev > 9) {
			printf("Invalid mmcdev: '%s'\n", envstr);
			return -EINVAL;
		}
	}

	envstr = env_get("mmcpart");
	if (envstr) {
		mmcpart = simple_strtoul(envstr, &eos, 10);
		if (*eos != '\0' || mmcpart < 1 || mmcpart > 32) {
			printf("Invalid mmcpart: '%s'\n", envstr);
			return -EINVAL;
		}
	}
	snprintf(dev_part, sizeof(dev_part), "%u:%u", mmcdev, mmcpart);

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
	karo_fdt_apply_overlays(fdt_addr);
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
static int karo_fdt_set_dr_mode(void *blob, int node, const char *mode)
{
	if (!fdtdec_get_is_enabled(blob, node)) {
		printf("usbotg interface is disabled\n");
		return 0;
	}

	printf("switching usbotg interface to %s mode\n", mode);
	return fdt_setprop_string(blob, node, "dr_mode", mode);
}

int ft_karo_common_setup(void *blob, bd_t *bd)
{
	int node;
	const char *serno_str;

	serno_str = env_get("serial#");
	if (serno_str) {
		printf("serial-number: %s\n", serno_str);
		fdt_increase_size(blob, 512);
		fdt_setprop(blob, 0, "serial-number", serno_str,
			    strlen(serno_str));
	}

	fdt_fixup_ethernet(blob);

	node = fdt_path_offset(blob, "usbotg");
	if (node > 0) {
		const char *otg_mode = env_get("otg_mode");

		if (strcmp(otg_mode, "peripheral") == 0 ||
		    strcmp(otg_mode, "device") == 0 ||
		    strcmp(otg_mode, "host") == 0) {
			karo_fdt_set_dr_mode(blob, node, otg_mode);
		} else if (!otg_mode || strcmp(otg_mode, "none") == 0) {
			printf("disabling usbotg interface\n");
			fdt_status_disabled(blob, node);
		} else if (otg_mode) {
			printf("Invalid otg_mode: '%s'\n", otg_mode);
		}
	}

	return 0;
}

__weak int ft_board_setup(void *blob, bd_t *bd)
{
	return ft_karo_common_setup(blob, bd);
}
#endif /* OF_BOARD_SETUP */
