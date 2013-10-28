/*
 * (C) Copyright 2012,2013 Lothar Waßmann <LW@KARO-electronics.de>
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

	if (working_fdt) {
		debug("DTB already loaded\n");
		return;
	}

	if (!fdt_addr) {
		fdt_addr = CONFIG_SYS_FDT_ADDR;
		printf("fdtaddr is not set; using default: %08lx\n",
			fdt_addr);
	}

	fdt = karo_fdt_load_dtb();
	if (fdt == NULL) {
		fdt = (void *)gd->fdt_blob;
		if (fdt == NULL) {
#ifdef CONFIG_OF_EMBED
			printf("Compiled in FDT not found\n");
#else
			printf("No FDT found\n");
#endif
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
	gd->fdt_blob = fdt;
	karo_set_fdtsize(fdt);
}

void karo_fdt_remove_node(void *blob, const char *node)
{
	int off = fdt_path_offset(blob, node);
	int ret;

	debug("Removing node '%s' from DT\n", node);

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
#ifdef CONFIG_MX28
	"fsl,imx28-lradc",
#endif
};

static void fdt_disable_tp_node(void *blob, const char *name)
{
	int offs = fdt_node_offset_by_compatible(blob, -1, name);

	if (offs < 0) {
		debug("node '%s' not found: %d\n", name, offs);
		return;
	}

	debug("Disabling node '%s'\n", name);
	fdt_set_node_status(blob, offs, FDT_STATUS_DISABLED, 0);
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
		fdt_disable_tp_node(blob, karo_touchpanels[i]);
	}
	karo_set_fdtsize(blob);
}

void karo_fdt_fixup_usb_otg(void *blob, const char *node, const char *phy)
{
	const char *otg_mode = getenv("otg_mode");
	int off;
	int ret;
	const uint32_t *ph;
	int disable_otg = 0;
	int disable_phy_pins = 1;

	debug("OTG mode is '%s'\n", otg_mode ? otg_mode : "<UNSET>");

	off = fdt_path_offset(blob, node);
	if (off < 0) {
		debug("Failed to find node %s\n", node);
		return;
	}

	if (otg_mode && (strcmp(otg_mode, "device") == 0 ||
				strcmp(otg_mode, "gadget") == 0)) {
		debug("Setting dr_mode to 'peripheral'\n");
		ret = fdt_setprop_string(blob, off, "dr_mode", "peripheral");
	} else if (otg_mode && strcmp(otg_mode, "host") == 0) {
		debug("Setting dr_mode to 'host'\n");
		ret = fdt_setprop_string(blob, off, "dr_mode", "host");
		disable_phy_pins = 0;
	} else if (otg_mode && strcmp(otg_mode, "otg") == 0) {
		debug("Setting dr_mode to 'host'\n");
		ret = fdt_setprop_string(blob, off, "dr_mode", "otg");
	} else {
		if (otg_mode && strcmp(otg_mode, "none") != 0)
			printf("Invalid 'otg_mode' setting '%s'; disabling usbotg port\n",
				otg_mode);
		disable_otg = 1;
		ret = 0;
	}

	if ((!disable_phy_pins && !disable_otg) || ret)
		goto out;

	if (disable_otg) {
		ret = fdt_set_node_status(blob, off, FDT_STATUS_DISABLED, 0);
		if (ret)
			goto out;
	}

	ph = fdt_getprop(blob, off, phy, NULL);
	if (ph == NULL) {
		printf("Failed to find '%s' phandle in node '%s'\n", phy,
			fdt_get_name(blob, off, NULL));
		goto out;
	}

	off = fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*ph));
	if (off <= 0) {
		printf("Failed to find '%s' node via phandle %04x\n",
			phy, fdt32_to_cpu(*ph));
		goto out;
	}

	if (disable_otg) {
		debug("Disabling usbphy\n");
		ret = fdt_set_node_status(blob, off, FDT_STATUS_DISABLED, 0);
	}
out:
	if (ret)
		printf("Failed to update usbotg: %d\n", ret);
	else
		debug("node '%s' updated\n", node);
	karo_set_fdtsize(blob);
}

static int karo_fdt_flexcan_enabled(void *blob)
{
	const char *can_ifs[] = {
		"can0",
		"can1",
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(can_ifs); i++) {
		const char *status;
		int off = fdt_path_offset(blob, can_ifs[i]);

		if (off < 0) {
			debug("node '%s' not found\n", can_ifs[i]);
			continue;
		}
		status = fdt_getprop(blob, off, "status", NULL);
		if (status && strcmp(status, "okay") == 0) {
			debug("%s is enabled\n", can_ifs[i]);
			return 1;
		}
	}
	debug("can driver is disabled\n");
	return 0;
}

static void karo_fdt_set_lcd_pins(void *blob, const char *name)
{
	int off = fdt_path_offset(blob, name);
	u32 ph;
	const struct fdt_property *pc;
	int len;

	if (off < 0)
		return;

	ph = fdt_create_phandle(blob, off);
	if (!ph)
		return;

	off = fdt_path_offset(blob, "display");
	if (off < 0)
		return;

	pc = fdt_get_property(blob, off, "pinctrl-0", &len);
	if (!pc || len < sizeof(ph))
		return;

	memcpy((void *)pc->data, &ph, sizeof(ph));
	fdt_setprop_cell(blob, off, "pinctrl-0", ph);
}

void karo_fdt_fixup_flexcan(void *blob, int xcvr_present)
{
	const char *xcvr_status = "disabled";

	if (xcvr_present) {
		if (karo_fdt_flexcan_enabled(blob)) {
			karo_fdt_set_lcd_pins(blob, "lcdif_23bit_pins_a");
			xcvr_status = "okay";
		} else {
			karo_fdt_set_lcd_pins(blob, "lcdif_24bit_pins_a");
		}
	} else {
		const char *otg_mode = getenv("otg_mode");

		if (otg_mode && (strcmp(otg_mode, "host") == 0))
			karo_fdt_enable_node(blob, "can1", 0);

		karo_fdt_set_lcd_pins(blob, "lcdif_24bit_pins_a");
	}
	fdt_find_and_setprop(blob, "/regulators/can-xcvr", "status",
			xcvr_status, strlen(xcvr_status) + 1, 1);
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

static int fdt_init_fb_mode(const void *blob, int off, struct fb_videomode *fb_mode)
{
	const uint32_t *prop;

	memset(fb_mode, 0, sizeof(*fb_mode));

	prop = fdt_getprop(blob, off, "clock-frequency", NULL);
	if (prop)
		fb_mode->pixclock = KHZ2PICOS(fdt32_to_cpu(*prop) / 1000);

	prop = fdt_getprop(blob, off, "hactive", NULL);
	if (prop)
		fb_mode->xres = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, off, "vactive", NULL);
	if (prop)
		fb_mode->yres = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, off, "hback-porch", NULL);
	if (prop)
		fb_mode->left_margin = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, off, "hsync-len", NULL);
	if (prop)
		fb_mode->hsync_len = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, off, "hfront-porch", NULL);
	if (prop)
		fb_mode->right_margin = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, off, "vback-porch", NULL);
	if (prop)
		fb_mode->upper_margin = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, off, "vsync-len", NULL);
	if (prop)
		fb_mode->vsync_len = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, off, "vfront-porch", NULL);
	if (prop)
		fb_mode->lower_margin = fdt32_to_cpu(*prop);

	prop = fdt_getprop(blob, off, "hsync-active", NULL);
	if (prop)
		fb_mode->sync |= *prop ? FB_SYNC_VERT_HIGH_ACT : 0;

	prop = fdt_getprop(blob, off, "vsync-active", NULL);
	if (prop)
		fb_mode->sync |= *prop ? FB_SYNC_VERT_HIGH_ACT : 0;

	prop = fdt_getprop(blob, off, "de-active", NULL);
	if (prop)
		fb_mode->sync |= *prop ? 0 : FB_SYNC_OE_LOW_ACT;

	prop = fdt_getprop(blob, off, "pixelclk-active", NULL);
	if (prop)
		fb_mode->sync |= *prop ? 0 : FB_SYNC_CLK_LAT_FALL;

	return 0;
}

static int fdt_update_native_fb_mode(void *blob, int off)
{
	int ret;
	uint32_t ph;

	ret = fdt_increase_size(blob, 64);
	if (ret) {
		printf("Warning: Failed to increase FDT size: %d\n", ret);
	}
	debug("Creating phandle at offset %d\n", off);
	ph = fdt_create_phandle(blob, off);
	if (!ph) {
		printf("Failed to create phandle for video timing\n");
		return -ENOMEM;
	}

	debug("phandle of %s @ %06x=%04x\n", fdt_get_name(blob, off, NULL),
		off, ph);
	off = fdt_parent_offset(blob, off);
	if (off < 0)
		return off;
	debug("parent offset=%06x\n", off);
	ret = fdt_setprop_cell(blob, off, "native-mode", ph);
	if (ret)
		printf("Failed to set property 'native-mode': %d\n", ret);
	karo_set_fdtsize(blob);
	return ret;
}

static int karo_fdt_find_video_timings(void *blob)
{
	int off = fdt_path_offset(blob, "display");
	const char *subnode = "display-timings";

	if (off < 0) {
		debug("Could not find node 'display' in FDT: %d\n", off);
		return off;
	}

	off = fdt_subnode_offset(blob, off, subnode);
	if (off < 0) {
		debug("Could not find node '%s' in FDT: %d\n", subnode, off);
	}
	return off;
}

int karo_fdt_get_fb_mode(void *blob, const char *name, struct fb_videomode *fb_mode)
{
	int off = karo_fdt_find_video_timings(blob);

	if (off < 0)
		return off;
	while (off > 0) {
		const char *n, *endp;
		int len, d = 1;

		off = fdt_next_node(blob, off, &d);
		if (off < 0)
			return off;
		if (d < 1)
			return -EINVAL;
		if (d > 2) {
			debug("Skipping node @ %04x %s depth %d\n", off, fdt_get_name(blob, off, NULL), d);
			continue;
		}
		debug("parsing subnode @ %04x %s depth %d\n", off, fdt_get_name(blob, off, NULL), d);

		n = fdt_getprop(blob, off, "panel-name", &len);
		if (!n) {
			n = fdt_get_name(blob, off, NULL);
			if (strcasecmp(n, name) == 0) {
				break;
			}
		} else {
			int found = 0;

			for (endp = n + len; n < endp; n += strlen(n) + 1) {
				debug("Checking panel-name '%s'\n", n);
				if (strcasecmp(n, name) == 0) {
					debug("Using node %s @ %04x\n",
						fdt_get_name(blob, off, NULL), off);
					found = 1;
					break;
				}
			}
			if (found)
				break;
		}
	}
	if (off > 0) {
		return fdt_init_fb_mode(blob, off, fb_mode);
	}
	return -EINVAL;
}

#define SET_FB_PROP(n, v) ({						\
	int ret;							\
	ret = fdt_setprop_u32(blob, off, n, v);				\
	if (ret) {							\
		printf("Failed to set property %s: %d\n", name, ret);	\
	}								\
	ret;								\
})


int karo_fdt_create_fb_mode(void *blob, const char *name,
			struct fb_videomode *fb_mode)
{
	int off = fdt_path_offset(blob, "display");
	int ret;
	const char *subnode = "display-timings";

	if (off < 0) {
		printf("'display' node not found in FDT\n");
		return off;
	}

	ret = fdt_increase_size(blob, 512);
	if (ret) {
		printf("Failed to increase FDT size: %d\n", ret);
		return ret;
	}

	printf("node '%s' offset=%d\n", "display", off);
	ret = fdt_subnode_offset(blob, off, subnode);
	if (ret < 0) {
		debug("Could not find node '%s' in FDT: %d\n", subnode, ret);
		ret = fdt_add_subnode(blob, off, subnode);
		if (ret < 0) {
			printf("Failed to add %s subnode: %d\n", subnode, ret);
			return ret;
		}
	}

	printf("node '%s' offset=%d\n", subnode, ret);
	ret = fdt_add_subnode(blob, ret, name);
	if (ret < 0) {
		printf("Failed to add %s subnode: %d\n", name, ret);
		return ret;
	}
	off = ret;

	ret = SET_FB_PROP("clock-frequency",
			PICOS2KHZ(fb_mode->pixclock) * 1000);
	if (ret)
		goto out;
	ret = SET_FB_PROP("hactive", fb_mode->xres);
	if (ret)
		goto out;
	ret = SET_FB_PROP("vactive", fb_mode->yres);
	if (ret)
		goto out;
	ret = SET_FB_PROP("hback-porch", fb_mode->left_margin);
	if (ret)
		goto out;
	ret = SET_FB_PROP("hsync-len", fb_mode->hsync_len);
	if (ret)
		goto out;
	ret = SET_FB_PROP("hfront-porch", fb_mode->right_margin);
	if (ret)
		goto out;
	ret = SET_FB_PROP("vback-porch", fb_mode->upper_margin);
	if (ret)
		goto out;
	ret = SET_FB_PROP("vsync-len", fb_mode->vsync_len);
	if (ret)
		goto out;
	ret = SET_FB_PROP("vfront-porch", fb_mode->lower_margin);
	if (ret)
		goto out;
	ret = SET_FB_PROP("hsync-active",
			fb_mode->sync & FB_SYNC_VERT_HIGH_ACT ? 1 : 0);
	if (ret)
		goto out;
	ret = SET_FB_PROP("vsync-active",
			fb_mode->sync & FB_SYNC_VERT_HIGH_ACT ? 1 : 0);
	if (ret)
		goto out;
	ret = SET_FB_PROP("de-active",
			!(fb_mode->sync & FB_SYNC_OE_LOW_ACT));
	if (ret)
		goto out;
	ret = SET_FB_PROP("pixelclk-active",
			!(fb_mode->sync & FB_SYNC_CLK_LAT_FALL));
out:
	karo_set_fdtsize(blob);
	return ret;
}

char *karo_fdt_set_display(char *video_mode, const char *lcd_path,
			const char *lvds_path)
{
	char *vm;
	int ret;
	int off;
	void *blob = (void *)gd->fdt_blob;

	if (video_mode == NULL)
		return NULL;

	off = fdt_path_offset(blob, "/aliases");
	ret = fdt_resize(blob);
	if (ret < 0) {
		printf("%s: Failed to resize FDT: %s\n",
			__func__, fdt_strerror(ret));
	}
	vm = strsep(&video_mode, ":");
	if (off < 0) {
		off = fdt_add_subnode(blob, off, "aliases");
		if (off < 0) {
			printf("%s: Failed to create 'aliases' node: %s\n",
				__func__, fdt_strerror(off));
			return video_mode ?: vm;
		}
	}
	if (video_mode != NULL) {
		char display[strlen(lvds_path) + sizeof("/lvds-channel@x")];

		strncpy(display, lvds_path, sizeof(display));
		if (strcmp(vm, "LVDS") == 0 ||
			strcmp(vm, "LVDS0") == 0) {
			strncat(display, "/lvds-channel@0", sizeof(display));
		} else if (strcmp(vm, "LVDS1") == 0) {
			strncat(display, "/lvds-channel@1", sizeof(display));
		} else {
			printf("%s: Syntax error in video_mode\n",
				__func__);
			return video_mode;
		}
		ret = fdt_setprop_string(blob, off, "display", display);
		debug("setprop_string(display='%s') returned %d\n", display, ret);
	} else {
		char display[strlen(lcd_path) + sizeof("/display@di0")];

		snprintf(display, sizeof(display), "%s/display@di0", lcd_path);

		ret = fdt_setprop_string(blob, off, "display",
					display);

		off = fdt_path_offset(blob, "lvds0");
		if (off >= 0) {
			ret = fdt_set_node_status(blob, off,
						FDT_STATUS_DISABLED, 0);
		}
		off = fdt_path_offset(blob, "lvds1");
		if (off >= 0) {
			ret = fdt_set_node_status(blob, off,
						FDT_STATUS_DISABLED, 0);
		}
		video_mode = vm;
	}
	if (ret) {
		printf("%s: failed to set 'display' alias: %s\n",
			__func__, fdt_strerror(ret));
	}
	return video_mode;
}

int karo_fdt_update_fb_mode(void *blob, const char *name)
{
	int off = fdt_path_offset(blob, "display");
	const char *subnode = "display-timings";

	if (off < 0)
		return off;

	if (name == NULL) {
		int ret;

		debug("Disabling node '%s' at %03x\n",
			fdt_get_name(blob, off, NULL), off);
		ret = fdt_set_node_status(blob, off, FDT_STATUS_DISABLED, 0);
		return ret;
	}

	off = fdt_subnode_offset(blob, off, subnode);
	if (off < 0) {
		debug("Could not find node '%s' in FDT: %d\n", subnode, off);
		return off;
	}
	while (off > 0) {
		const char *n, *endp;
		int len, d = 1;

		off = fdt_next_node(blob, off, &d);
		if (off < 0)
			return off;
		if (d < 1)
			return -EINVAL;
		if (d > 2) {
			debug("Skipping node @ %04x %s depth %d\n", off, fdt_get_name(blob, off, NULL), d);
			continue;
		}
		debug("parsing subnode @ %04x %s depth %d\n", off, fdt_get_name(blob, off, NULL), d);

		n = fdt_getprop(blob, off, "panel-name", &len);
		if (!n) {
			n = fdt_get_name(blob, off, NULL);
			if (strcasecmp(n, name) == 0) {
				break;
			}
		} else {
			int found = 0;

			for (endp = n + len; n < endp; n += strlen(n) + 1) {
				debug("Checking panel-name '%s'\n", n);
				if (strcasecmp(n, name) == 0) {
					debug("Using node %s @ %04x\n",
						fdt_get_name(blob, off, NULL), off);
					found = 1;
					break;
				}
			}
			if (found)
				break;
		}
	}
	if (off > 0)
		return fdt_update_native_fb_mode(blob, off);
	return off;
}

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
	ret = find_dev_and_part(part, &dev, &part_num, &part_info);
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

void *karo_fdt_load_dtb(void)
{
	int ret;
	void *fdt = (void *)getenv_ulong("fdtaddr", 16, CONFIG_SYS_FDT_ADDR);

	if (tstc()) {
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
