/*
 * (C) Copyright 2012-2014 Lothar Waßmann <LW@KARO-electronics.de>
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

static void karo_set_fdtsize(void *fdt)
{
	size_t fdtsize = getenv_ulong("fdtsize", 16, 0);

	if (fdtsize == fdt_totalsize(fdt)) {
		return;
	}
	debug("FDT size changed from %u to %u\n",
		fdtsize, fdt_totalsize(fdt));
	setenv_hex("fdtsize", fdt_totalsize(fdt));
}

static void *karo_fdt_load_dtb(void)
{
	int ret;
	void *fdt;

	if (getenv("fdtaddr") == NULL)
		setenv_hex("fdtaddr", CONFIG_SYS_FDT_ADDR);
	fdt = (void *)getenv_ulong("fdtaddr", 16, CONFIG_SYS_FDT_ADDR);

	if (had_ctrlc()) {
		printf("aborting DTB load\n");
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

void karo_fdt_move_fdt(void)
{
	void *fdt;
	unsigned long fdt_addr = getenv_ulong("fdtaddr", 16, 0);

	if (working_fdt) {
		debug("DTB already loaded\n");
		return;
	}

	setenv("fdtsize", 0);

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
	gd->fdt_blob = fdt;
	karo_set_fdtsize(fdt);
}

void karo_fdt_remove_node(void *blob, const char *node)
{
	int off = fdt_path_offset(blob, node);
	int ret;

	debug("Removing node '%s' from DT\n", node);

	if (off < 0) {
		printf("Could not find node '%s': %s\n", node,
			fdt_strerror(off));
	} else {
		ret = fdt_del_node(blob, off);
		if (ret)
			printf("Failed to remove node '%s': %s\n",
				node, fdt_strerror(ret));
	}
	karo_set_fdtsize(blob);
}

void karo_fdt_enable_node(void *blob, const char *node, int enable)
{
	int off = fdt_path_offset(blob, node);

	debug("%sabling node '%s'\n", enable ? "En" : "Dis", node);
	if (off < 0) {
		printf("Could not find node '%s': %s\n", node,
			fdt_strerror(off));
		return;
	}
	fdt_set_node_status(blob, off,
			enable ? FDT_STATUS_OKAY : FDT_STATUS_DISABLED, 0);

	karo_set_fdtsize(blob);
}

static void fdt_disable_tp_node(void *blob, const char *name)
{
	int offs = fdt_node_offset_by_compatible(blob, -1, name);

	while (offs >= 0) {
		debug("Disabling node '%s'\n", name);
		fdt_set_node_status(blob, offs, FDT_STATUS_DISABLED, 0);
		offs = fdt_node_offset_by_compatible(blob, offs, name);
	}
}

void karo_fdt_fixup_touchpanel(void *blob, const char *panels[],
			size_t num_panels)
{
	int i;
	const char *model = getenv("touchpanel");

	for (i = 0; i < num_panels; i++) {
		const char *tp = panels[i];

		if (model != NULL) {
			if (strcmp(model, tp) == 0)
				continue;
			while (tp != NULL) {
				if (*tp != '\0' && strcmp(model, tp + 1) == 0)
					break;
				tp = strpbrk(tp + 1, ",-");
			}
			if (tp != NULL)
				continue;
		}
		fdt_disable_tp_node(blob, panels[i]);
	}
	karo_set_fdtsize(blob);
}

static int karo_fdt_disable_node_phandle(void *blob, const char *parent,
					const char *name)
{
	const uint32_t *ph;
	int off;

	off = fdt_path_offset(blob, parent);
	if (off < 0) {
		printf("Failed to find node '%s'\n", parent);
		return off;
	}

	ph = fdt_getprop(blob, off, name, NULL);
	if (ph == NULL) {
		debug("Failed to find '%s' phandle in node '%s'\n", name,
			fdt_get_name(blob, off, NULL));
		return -FDT_ERR_NOTFOUND;
	}

	off = fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*ph));
	if (off <= 0) {
		printf("Failed to find '%s' node via phandle %04x\n",
			name, fdt32_to_cpu(*ph));
		return -FDT_ERR_NOTFOUND;
	}
	return fdt_set_node_status(blob, off, FDT_STATUS_DISABLED, 0);
}

void karo_fdt_fixup_usb_otg(void *blob, const char *node, const char *phy,
			const char *phy_supply)
{
	const char *otg_mode = getenv("otg_mode");
	int off;
	int ret;
	int disable_otg = 0;
	int disable_phy_pins = 0;

	debug("OTG mode is '%s'\n", otg_mode ? otg_mode : "<UNSET>");

	off = fdt_path_offset(blob, node);
	if (off < 0) {
		debug("Failed to find node %s\n", node);
		return;
	}

	if (otg_mode && (strcasecmp(otg_mode, "device") == 0 ||
				strcasecmp(otg_mode, "gadget") == 0)) {
		debug("Setting dr_mode to 'peripheral'\n");
		ret = fdt_setprop_string(blob, off, "dr_mode", "peripheral");
		disable_phy_pins = 1;
	} else if (otg_mode && strcasecmp(otg_mode, "host") == 0) {
		debug("Setting dr_mode to 'host'\n");
		ret = fdt_setprop_string(blob, off, "dr_mode", "host");
	} else if (otg_mode && strcasecmp(otg_mode, "otg") == 0) {
		debug("Setting dr_mode to 'otg'\n");
		ret = fdt_setprop_string(blob, off, "dr_mode", "otg");
	} else {
		if (otg_mode && strcasecmp(otg_mode, "none") != 0)
			printf("Invalid 'otg_mode' setting '%s'; disabling usbotg port\n",
				otg_mode);
		disable_otg = 1;
		ret = 0;
	}

	if ((!disable_phy_pins && !disable_otg) || ret)
		goto out;

	ret = karo_fdt_disable_node_phandle(blob, node, phy_supply);
	if (ret && ret == -FDT_ERR_NOTFOUND) {
		const uint32_t *ph;

		ph = fdt_getprop(blob, off, phy, NULL);
		if (ph == NULL) {
			printf("Failed to find '%s' phandle in node '%s'\n",
				phy, node);
			ret = -FDT_ERR_NOTFOUND;
			goto out;
		}
		off = fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*ph));
		if (off < 0) {
			printf("Failed to find '%s' node via phandle %04x\n",
				phy, fdt32_to_cpu(*ph));
			ret = off;
			goto out;
		}
		ph = fdt_getprop(blob, off, phy_supply, NULL);
		if (ph == NULL) {
			debug("Failed to find '%s' phandle in node '%s'\n",
				phy_supply, fdt_get_name(blob, off, NULL));
			ret = -FDT_ERR_NOTFOUND;
			goto disable_otg;
		}
		ret = fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*ph));
		if (ret > 0) {
			debug("Disabling node %s via phandle %s:%s\n",
				fdt_get_name(blob, ret, NULL),
				fdt_get_name(blob, off, NULL), phy_supply);
			ret = fdt_set_node_status(blob, ret,
						FDT_STATUS_DISABLED, 0);
		}
	}
	if (ret && ret != -FDT_ERR_NOTFOUND)
		goto out;

disable_otg:
	if (disable_otg) {
		debug("Disabling '%s'\n", fdt_get_name(blob, off, NULL));
		ret = fdt_set_node_status(blob, off, FDT_STATUS_DISABLED, 0);
		if (ret > 0)
			ret = karo_fdt_disable_node_phandle(blob, node, phy);
	} else if (disable_phy_pins) {
		debug("Removing '%s' from node '%s'\n", phy_supply,
			fdt_get_name(blob, off, NULL));
		ret = fdt_delprop(blob, off, phy_supply);
	}

out:
	if (ret && ret != -FDT_ERR_NOTFOUND)
		printf("Failed to update usbotg: %s\n", fdt_strerror(ret));
	else
		debug("node '%s' updated\n", node);
	karo_set_fdtsize(blob);
}

static inline int karo_fdt_flexcan_enabled(void *blob)
{
	static const char *can_ifs[] = {
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

static inline void karo_fdt_set_lcd_pins(void *blob, const char *name)
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
	int ret;
	const char *xcvr_status = "disabled";
	const char *otg_mode = getenv("otg_mode");

	if (xcvr_present) {
		if (karo_fdt_flexcan_enabled(blob)) {
			if (!is_lvds()) {
				debug("Changing LCD to use 23bits only\n");
				karo_fdt_set_lcd_pins(blob, "lcdif-23bit-pins-a");
				/* handle legacy alias name */
				karo_fdt_set_lcd_pins(blob, "lcdif_23bit_pins_a");
				xcvr_status = NULL;
			}
		} else if (!is_lvds()) {
			debug("Changing LCD to use 24bits\n");
			karo_fdt_set_lcd_pins(blob, "lcdif-24bit-pins-a");
			/* handle legacy alias name */
			karo_fdt_set_lcd_pins(blob, "lcdif_24bit_pins_a");
		}
	} else {
		int off = fdt_path_offset(blob, "can0");

		if (off >= 0)
			fdt_delprop(blob, off, "xceiver-supply");
		off = fdt_path_offset(blob, "can1");
		if (off >= 0)
			fdt_delprop(blob, off, "xceiver-supply");
		if (!is_lvds()) {
			karo_fdt_set_lcd_pins(blob, "lcdif-24bit-pins-a");
			/* handle legacy alias name */
			karo_fdt_set_lcd_pins(blob, "lcdif_24bit_pins_a");
		}
	}

	if (otg_mode && strcasecmp(otg_mode, "host") == 0)
		karo_fdt_enable_node(blob, "can1", 0);

	if (xcvr_status) {
		debug("Disabling CAN XCVR\n");
		ret = fdt_find_and_setprop(blob, "reg-can-xcvr", "status",
					xcvr_status, strlen(xcvr_status) + 1, 1);
		if (ret == -FDT_ERR_NOTFOUND || ret == -FDT_ERR_BADPATH)
			ret = fdt_find_and_setprop(blob, "reg_can_xcvr", "status",
						   xcvr_status, strlen(xcvr_status) + 1, 1);
		if (ret && ret != -FDT_ERR_NOTFOUND && ret != -FDT_ERR_BADPATH)
			printf("Failed to disable CAN transceiver switch: %s\n",
				fdt_strerror(ret));
	}
}

void karo_fdt_del_prop(void *blob, const char *compat, u32 offs,
			const char *propname)
{
	int offset = -1;
	const fdt32_t *reg = NULL;

	while (1) {
		offset = fdt_node_offset_by_compatible(blob, offset, compat);
		if (offset <= 0)
			return;

		reg = fdt_getprop(blob, offset, "reg", NULL);
		if (reg == NULL)
			return;

		if (fdt32_to_cpu(*reg) == offs)
			break;
	}
	debug("Removing property '%s' from node %s@%x\n",
		propname, compat, offs);
	fdt_delprop(blob, offset, propname);

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
		fb_mode->sync |= *prop ? FB_SYNC_CLK_LAT_FALL : 0;

	return 0;
}

static int fdt_update_native_fb_mode(void *blob, int off)
{
	int ret;
	uint32_t ph;

	ret = fdt_increase_size(blob, 64);
	if (ret) {
		printf("Warning: Failed to increase FDT size: %s\n",
			fdt_strerror(ret));
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
		printf("Failed to set property 'native-mode': %s\n",
			fdt_strerror(ret));
	karo_set_fdtsize(blob);
	return ret;
}

static int karo_fdt_find_video_timings(void *blob)
{
	int off = fdt_path_offset(blob, "display");
	const char *subnode = "display-timings";

	if (off < 0) {
		debug("Could not find node 'display' in FDT: %s\n",
			fdt_strerror(off));
		return off;
	}

	off = fdt_subnode_offset(blob, off, subnode);
	if (off < 0) {
		debug("Could not find node '%s' in FDT: %s\n", subnode,
			fdt_strerror(off));
	}
	return off;
}

static int karo_fdt_check_panel_name(const char *pn, const char *name, int len)
{
	const char *endp = pn + len;

	if (len < 0)
		return 0;
	while (pn < endp) {
		if (strcasecmp(pn, name) == 0)
			return 1;
		pn += strlen(pn) + 1;
	}
	return 0;
}

static int karo_fdt_find_panel(const void *blob, int off, const char *name)
{
	debug("Searching panel '%s'\n", name);
	while (off > 0) {
		const char *pn;
		int d = 1;
		int len;

		off = fdt_next_node(blob, off, &d);
		if (off < 0)
			return off;
		if (d < 1)
			return -EINVAL;
		if (d > 2) {
			debug("Skipping node @ %04x %s depth %d\n", off,
				fdt_get_name(blob, off, NULL), d);
			continue;
		}

		pn = fdt_get_name(blob, off, NULL);
		debug("Checking node name '%s'\n", pn);
		if (strcasecmp(pn, name) == 0)
			break;
		pn = fdt_getprop(blob, off, "u-boot,panel-name", &len);
		if (!pn)
			continue;
		if (karo_fdt_check_panel_name(pn, name, len))
			break;
	}
	if (off > 0)
		debug("Found LCD panel: '%s' @ off %03x\n",
		      fdt_get_name(blob, off, NULL), off);
	return off;
}

int karo_fdt_get_fb_mode(void *blob, const char *name,
			 struct fb_videomode *fb_mode)
{
	int off = karo_fdt_find_video_timings(blob);

	if (off < 0)
		return off;
	off = karo_fdt_find_panel(blob, off, name);
	if (off > 0) {
		return fdt_init_fb_mode(blob, off, fb_mode);
	}
	return -EINVAL;
}

#define SET_FB_PROP(n, v) ({						\
	int ret;							\
	ret = fdt_setprop_u32(blob, off, n, v);				\
	if (ret) {							\
		printf("Failed to set property %s: %s\n", name,		\
			fdt_strerror(ret));				\
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
		printf("Failed to increase FDT size: %s\n", fdt_strerror(ret));
		return ret;
	}

	ret = fdt_subnode_offset(blob, off, subnode);
	if (ret < 0) {
		debug("Could not find node '%s' in FDT: %s\n", subnode,
			fdt_strerror(ret));
		ret = fdt_add_subnode(blob, off, subnode);
		if (ret < 0) {
			printf("Failed to add %s subnode: %s\n", subnode,
				fdt_strerror(ret));
			return ret;
		}
	}

	ret = fdt_add_subnode(blob, ret, name);
	if (ret < 0) {
		printf("Failed to add %s subnode: %s\n", name,
			fdt_strerror(ret));
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

static const char *karo_panel_timing_props[] = {
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

	for (i = 0; i < ARRAY_SIZE(karo_panel_timing_props); i++) {
		const char *name = karo_panel_timing_props[i];
		int len;
		int ret;
		const void *val;
		bool restart;

		val = fdt_getprop(fdt, src, name, &len);
		if (!val) {
			if (fdt_getprop(fdt, dest, name, NULL)) {
				printf("Removing '%s' from '%s'\n", name,
				       fdt_get_name(fdt, dest, NULL));
				fdt_delprop(fdt, dest, name);
				return -EAGAIN;
			}
			continue;
		}
		if (len != sizeof(u32)) {
			printf("Property '%s' has invalid size %u\n",
			       name, len);
			return -EINVAL;
		}
		debug("setting '%s' to <0x%08x>\n", name, be32_to_cpup(val));

		restart = !fdt_getprop(fdt, dest, name, &len);
		restart |= len != sizeof(u32);
		/* DTB offsets will change when adding a new property */
		if (restart) {
			ret = fdt_increase_size(fdt, len);
			if (ret) {
				printf("Failed to increase FDT size by %u: %s\n", len,
				       fdt_strerror(ret));
				return -ENOMEM;
			}
			printf("Adding new property '%s' to '%s'\n",
			       name, fdt_get_name(fdt, dest, NULL));
		}
		ret = fdt_setprop_u32(fdt, dest, name, be32_to_cpup(val));
		if (ret) {
			printf("Failed to set %s property: %s\n", name,
			       fdt_strerror(ret));
			return -EINVAL;
		}
		if (restart)
			return -EAGAIN;
	}
	return 0;
}

int karo_fdt_update_fb_mode(void *blob, const char *name,
			    const char *panel_name)
{
	int ret;
	int off = fdt_path_offset(blob, "display");
	int dt_node;
	int panel_off = -1;
	const char *subnode = "display-timings";
	size_t i = 0;

	if (panel_name)
		panel_off = fdt_path_offset(blob, panel_name);

	if (off < 0 && panel_off < 0)
		return off;

	if (name == NULL) {
		debug("Disabling node '%s' at %03x\n",
			fdt_get_name(blob, off, NULL), off);
		ret = fdt_set_node_status(blob, off, FDT_STATUS_DISABLED, 0);
		if (ret)
			printf("Failed to disable node '%s': %s\n",
			       fdt_get_name(blob, off, NULL),
			       fdt_strerror(ret));
		if (!panel_name)
			return ret;

		panel_off = fdt_path_offset(blob, panel_name);
		if (panel_off < 0)
			return 0;
		return fdt_set_node_status(blob, panel_off,
					   FDT_STATUS_DISABLED, 0);
	}

	off = fdt_subnode_offset(blob, off, subnode);
	if (off < 0) {
		debug("Could not find node '%s' in FDT: %s\n", subnode,
			fdt_strerror(off));
		return off;
	}
	off = karo_fdt_find_panel(blob, off, name);
	if (off > 0)
		ret = fdt_update_native_fb_mode(blob, off);
	else
		ret = 0;
	if (!panel_name)
		return ret;
 restart:
	dt_node = fdt_path_offset(blob, "display/display-timings");
	off = karo_fdt_find_panel(blob, dt_node, name);
	panel_off = fdt_path_offset(blob, panel_name);
	if (panel_off > 0) {
		int node = fdt_subnode_offset(blob, dt_node, name);

		if (node < 0) {
			printf("Warning: No '%s' subnode found in 'display-timings'\n",
			       name);
			return -ENOENT;
		}
		if (fdt_node_check_compatible(blob, panel_off, "panel-dpi") == 0) {
			int timing_node = fdt_subnode_offset(blob, panel_off,
							     "panel-timing");

			if (timing_node < 0) {
				printf("Warning: No 'panel-timing' subnode found\n");
				return -ENOENT;
			}

			if (i == 0)
				printf("Copying video timing from %s\n", name);
			ret = karo_fixup_panel_timing(blob,
						      timing_node,
						      node);
			if (ret == -EAGAIN) {
				if (i++ > ARRAY_SIZE(karo_panel_timing_props))
					return -EINVAL;
				goto restart;
			}
		} else {
			char *pn;

			name = fdt_getprop(blob, off, "u-boot,panel-name",
					   NULL);
			if (!name)
				return 0;

			pn = strdup(name);
			if (!pn)
				return -ENOMEM;
			debug("%s@%d: Updating 'compatible' property of '%s' from '%s' to '%s'\n",
			      __func__, __LINE__, fdt_get_name(blob, panel_off, NULL),
			      (char *)fdt_getprop(blob, panel_off, "compatible", NULL),
			      pn);

			ret = fdt_setprop_string(blob, panel_off, "compatible",
						 pn);
			if (ret)
				printf("Failed to set 'compatible' property of node '%s': %s\n",
				       fdt_get_name(blob, panel_off, NULL),
				       fdt_strerror(off));
			free(pn);
		}
	}
	return ret;
}

#ifdef CONFIG_SYS_LVDS_IF
int karo_fdt_get_lcd_bus_width(const void *blob, int default_width)
{
	int off = fdt_path_offset(blob, "display");

	if (off >= 0) {
		const uint32_t *prop;

		prop = fdt_getprop(blob, off, "fsl,data-width", NULL);
		if (prop)
			return fdt32_to_cpu(*prop);
	}
	return default_width;
}

int karo_fdt_get_lvds_mapping(const void *blob, int default_mapping)
{
	int off = fdt_path_offset(blob, "display");

	if (off >= 0) {
		const char *prop;

		prop = fdt_getprop(blob, off, "fsl,data-mapping", NULL);
		if (prop)
			return strcmp(prop, "jeida") == 0;
	}
	return default_mapping;
}

u8 karo_fdt_get_lvds_channels(const void *blob)
{
	static const char *lvds_chans[] = {
		"lvds0",
		"lvds1",
	};
	u8 lvds_chan_mask = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(lvds_chans); i++) {
		const char *status;
		int off = fdt_path_offset(blob, lvds_chans[i]);

		if (off < 0)
			continue;

		status = fdt_getprop(blob, off, "status", NULL);
		if (status && strcmp(status, "okay") == 0) {
			debug("%s is enabled\n", lvds_chans[i]);
			lvds_chan_mask |= 1 << i;
		}
	}
	return lvds_chan_mask;
}
#endif

int karo_fdt_get_backlight_polarity(const void *blob)
{
#ifdef CONFIG_SYS_LVDS_IF
	const char *backlight_node = "/backlight0";
#else
	const char *backlight_node = "/backlight";
#endif
	int off = fdt_path_offset(blob, "backlight"); /* first try alias */
	const struct fdt_property *prop;
	int len;

	if (off < 0) {
		/*
		 * if no 'backlight' alias exists try finding '/backlight0'
		 * or '/backlight' depending on LVDS or not
		 */
		off = fdt_path_offset(blob, backlight_node);
		if (off < 0) {
			printf("%s node not found in DT\n", backlight_node);
			return off;
		}
	}

	prop = fdt_get_property(blob, off, "pwms", &len);
	if (!prop)
		printf("'pwms' property not found\n");
	else
		debug("'pwms' property has len %d\n", len);

	len /= sizeof(u32);
	if (prop && len > 3) {
		const u32 *data = (const u32 *)prop->data;
		return fdt32_to_cpu(data[3]) == 0;
	}
	return 0;
}
