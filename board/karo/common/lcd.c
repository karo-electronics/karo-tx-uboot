#include <common.h>
#include <linux/fb.h>
#ifdef CONFIG_VIDEO_IPUV3
#include <mxcfb.h>
#endif

#include "karo.h"

static struct fb_videomode karo_tx_fb_modes[] = {
#ifndef CONFIG_SYS_LVDS_IF
	{
		/* Standard VGA timing */
		.name		= "VGA",
		.refresh	= 60,
		.xres		= 640,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(25175),
		.left_margin	= 48,
		.hsync_len	= 96,
		.right_margin	= 16,
		.upper_margin	= 31,
		.vsync_len	= 2,
		.lower_margin	= 12,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ETV570 640 x 480 display. Syncs low active,
		 * DE high active, 115.2 mm x 86.4 mm display area
		 * VGA compatible timing
		 */
		.name		= "ETV570",
		.refresh	= 60,
		.xres		= 640,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(25175),
		.left_margin	= 114,
		.hsync_len	= 30,
		.right_margin	= 16,
		.upper_margin	= 32,
		.vsync_len	= 3,
		.lower_margin	= 10,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0350G0DH6 320 x 240 display.
		 * 70.08 mm x 52.56 mm display area.
		 */
		.name		= "ET0350",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= KHZ2PICOS(6500),
		.left_margin	= 68 - 34,
		.hsync_len	= 34,
		.right_margin	= 20,
		.upper_margin	= 18 - 3,
		.vsync_len	= 3,
		.lower_margin	= 4,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0430G0DH6 480 x 272 display.
		 * 95.04 mm x 53.856 mm display area.
		 */
		.name		= "ET0430",
		.refresh	= 60,
		.xres		= 480,
		.yres		= 272,
		.pixclock	= KHZ2PICOS(9000),
		.left_margin	= 2,
		.hsync_len	= 41,
		.right_margin	= 2,
		.upper_margin	= 2,
		.vsync_len	= 10,
		.lower_margin	= 2,
	},
	{
		/* Emerging ET0500G0DH6 800 x 480 display.
		 * 109.6 mm x 66.4 mm display area.
		 */
		.name		= "ET0500",
		.refresh	= 60,
		.xres		= 800,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),
		.left_margin	= 216 - 128,
		.hsync_len	= 128,
		.right_margin	= 1056 - 800 - 216,
		.upper_margin	= 35 - 2,
		.vsync_len	= 2,
		.lower_margin	= 525 - 480 - 35,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ETQ570G0DH6 320 x 240 display.
		 * 115.2 mm x 86.4 mm display area.
		 */
		.name		= "ETQ570",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= KHZ2PICOS(6400),
		.left_margin	= 38,
		.hsync_len	= 30,
		.right_margin	= 30,
		.upper_margin	= 16, /* 15 according to datasheet */
		.vsync_len	= 3, /* TVP -> 1>x>5 */
		.lower_margin	= 4, /* 4.5 according to datasheet */
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0700G0DH6 800 x 480 display.
		 * 152.4 mm x 91.44 mm display area.
		 */
		.name		= "ET0700",
		.refresh	= 60,
		.xres		= 800,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),
		.left_margin	= 216 - 128,
		.hsync_len	= 128,
		.right_margin	= 1056 - 800 - 216,
		.upper_margin	= 35 - 2,
		.vsync_len	= 2,
		.lower_margin	= 525 - 480 - 35,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET070001DM6 800 x 480 display.
		 * 152.4 mm x 91.44 mm display area.
		 */
		.name		= "ET070001DM6",
		.refresh	= 60,
		.xres		= 800,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),
		.left_margin	= 216 - 128,
		.hsync_len	= 128,
		.right_margin	= 1056 - 800 - 216,
		.upper_margin	= 35 - 2,
		.vsync_len	= 2,
		.lower_margin	= 525 - 480 - 35,
		.sync		= 0,
	},
#else
	{
		/* HannStar HSD100PXN1
		 * 202.7m mm x 152.06 mm display area.
		 */
		.name		= "HSD100PXN1",
		.refresh	= 60,
		.xres		= 1024,
		.yres		= 768,
		.pixclock	= KHZ2PICOS(65000),
		.left_margin	= 0,
		.hsync_len	= 0,
		.right_margin	= 320,
		.upper_margin	= 0,
		.vsync_len	= 0,
		.lower_margin	= 38,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
#endif
	{
		/* unnamed entry for assigning parameters parsed from 'video_mode' string */
		.refresh	= 60,
		.left_margin	= 48,
		.hsync_len	= 96,
		.right_margin	= 16,
		.upper_margin	= 31,
		.vsync_len	= 2,
		.lower_margin	= 12,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
};

int karo_get_fb_mode(const char *name, struct fb_videomode **mode)
{
	size_t i;

	if (name == NULL) {
		if (mode)
			*mode = karo_tx_fb_modes;
		return ARRAY_SIZE(karo_tx_fb_modes);
	}
	for (i = 0; i < ARRAY_SIZE(karo_tx_fb_modes); i++) {
		struct fb_videomode *m = &karo_tx_fb_modes[i];

		if (m->name && strcmp(m->name, name) == 0) {
			if (mode)
				*mode = m;
			return i;
		}
	}
	if (mode)
		*mode = NULL;
	return -ENOENT;
}
