/* SPDX-License-Identifier: GPL-2.0
 *
 * (C) Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

struct node_info;

void karo_env_cleanup(void);
void karo_fdt_move_fdt(void);

#ifdef CONFIG_DM_VIDEO
struct fb_videomode;

int karo_fdt_get_backlight_polarity(const void *blob);
int karo_fdt_get_fb_mode(const char *name, struct fb_videomode *fb_mode);
void karo_fixup_lcd_panel(const char *videomode);
#else
static inline void karo_fixup_lcd_panel(const char *videomode)
{
}
#endif
