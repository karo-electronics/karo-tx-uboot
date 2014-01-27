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
struct fb_videomode;

#ifdef CONFIG_OF_LIBFDT
void karo_fdt_remove_node(void *blob, const char *node);
void karo_fdt_move_fdt(void);
void karo_fdt_fixup_touchpanel(void *blob, const char *panels[],
			size_t num_panels);
void karo_fdt_fixup_usb_otg(void *blob, const char *node, const char *phy);
void karo_fdt_fixup_flexcan(void *blob, int xcvr_present);
void karo_fdt_del_prop(void *blob, const char *compat, phys_addr_t offs,
		const char *prop);
void karo_fdt_enable_node(void *blob, const char *node, int enable);
int karo_fdt_get_fb_mode(void *blob, const char *name,
		struct fb_videomode *fb_mode);
int karo_fdt_update_fb_mode(void *blob, const char *name);
int karo_fdt_create_fb_mode(void *blob, const char *name,
			struct fb_videomode *mode);
int karo_fdt_get_lcd_bus_width(const void *blob, int default_width);
int karo_fdt_get_lvds_mapping(const void *blob, int default_mapping);
u8 karo_fdt_get_lvds_channels(const void *blob);
int karo_fdt_get_backlight_polarity(const void *blob);
#else
static inline void karo_fdt_remove_node(void *blob, const char *node)
{
}
static inline void karo_fdt_move_fdt(void)
{
}
static inline void karo_fdt_fixup_touchpanel(void *blob, const char *panels[],
					size_t num_panels)
{
}
static inline void karo_fdt_fixup_usb_otg(void *blob, const char *node,
					const char *phy)
{
}
static inline void karo_fdt_fixup_flexcan(void *blob, int xcvr_present)
{
}
static inline void karo_fdt_del_prop(void *blob, const char *compat,
				phys_addr_t offs, const char *prop)
{
}
static inline void karo_fdt_enable_node(void *blob, const char *node,
					int enable)
{
}
static inline int karo_fdt_get_fb_mode(void *blob, const char *name,
				struct fb_videomode *fb_mode)
{
	return 0;
}
static inline int karo_fdt_update_fb_mode(void *blob, const char *name)
{
	return 0;
}
static inline int karo_fdt_create_fb_mode(void *blob,
					const char *name,
					struct fb_videomode *mode)
{
	return 0;
}
static inline int karo_fdt_get_lcd_bus_width(const void *blob, int default_width)
{
	return default_width;
}
static inline int karo_fdt_get_lvds_mapping(const void *blob, int default_mapping)
{
	return default_mapping;
}
static inline u8 karo_fdt_get_lvds_channels(const void *blob)
{
	return 0;
}
static inline int karo_fdt_get_backlight_polarity(const void *blob)
{
	return getenv_yesno("backlight_polarity");
}
#endif

static inline const char *karo_get_vmode(const char *video_mode)
{
	const char *vmode = NULL;

	if (video_mode == NULL || strlen(video_mode) == 0)
		return NULL;

	vmode = strchr(video_mode, ':');
	return vmode ? vmode + 1 : video_mode;
}

#ifdef CONFIG_SPLASH_SCREEN
int karo_load_splashimage(int mode);
#else
static inline int karo_load_splashimage(int mode)
{
	return 0;
}
#endif

#ifdef CONFIG_CMD_NAND
int karo_load_nand_part(const char *part, void *addr, size_t len);
#else
static inline int karo_load_nand_part(const char *part, void *addr, size_t len)
{
	return -EOPNOTSUPP;
}
#endif

#ifdef CONFIG_CMD_MMC
int karo_load_mmc_part(const char *part, void *addr, size_t len);
#else
static inline int karo_load_mmc_part(const char *part, void *addr, size_t len)
{
	return -EOPNOTSUPP;
}
#endif

static inline int karo_load_part(const char *part, void *addr, size_t len)
{
	int ret;

	ret = karo_load_nand_part(part, addr, len);
	if (ret == -EOPNOTSUPP)
		return karo_load_mmc_part(part, addr, len);
	return ret;
}
