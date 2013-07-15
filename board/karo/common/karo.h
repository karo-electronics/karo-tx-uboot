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

#ifdef CONFIG_OF_LIBFDT
void karo_fdt_remove_node(void *blob, const char *node);
void karo_fdt_move_fdt(void);
void karo_fdt_fixup_touchpanel(void *blob);
void karo_fdt_fixup_usb_otg(void *blob, const char *node, const char *phy);
void karo_fdt_del_prop(void *blob, const char *compat, phys_addr_t offs,
		const char *prop);
void karo_fdt_enable_node(void *blob, const char *node, int enable);
void *karo_fdt_load_dtb(void);
int karo_fdt_get_fb_mode(void *blob, const char *name,
		struct fb_videomode *fb_mode);
int karo_fdt_update_fb_mode(void *blob, const char *name);
#else
static inline void karo_fdt_remove_node(void *blob, const char *node)
{
}
static inline void karo_fdt_move_fdt(void)
{
}
static inline void karo_fdt_fixup_touchpanel(void *blob)
{
}
static inline void karo_fdt_fixup_usb_otg(void *blob, const char *node,
					const char *phy)
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
static inline void *karo_fdt_load_dtb(void)
{
	return NULL;
}
static inline int karo_fdt_get_fb_mode(void *blob, const char *name,
				struct fb_videomode *fb_mode)
{
	return NULL;
}
static inline int karo_fdt_update_fb_mode(void *blob, const char *name)
{
	return 0;
}
#endif

int karo_load_splashimage(int mode);
