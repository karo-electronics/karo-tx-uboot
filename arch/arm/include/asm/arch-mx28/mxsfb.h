/*
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * LCD driver for i.MX28
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MXSFB_H
#define __MXSFB_H

#include <linux/list.h>
#include <linux/fb.h>

#define fourcc(a, b, c, d)	(((__u32)(a) << 0) |			\
				((__u32)(b) << 8) |			\
				((__u32)(c) << 16) |			\
				((__u32)(d) << 24))

/*
 * Pixel formats are defined with ASCII FOURCC code. The pixel format codes are
 * the same used by V4L2 API.
 */

#define PIX_FMT_RGB332  fourcc('R', 'G', 'B', '1')  /*<  8  RGB-3-3-2    */
#define PIX_FMT_RGB555  fourcc('R', 'G', 'B', 'O')  /*< 16  RGB-5-5-5    */
#define PIX_FMT_RGB565  fourcc('R', 'G', 'B', 'P')  /*< 1 6  RGB-5-6-5   */
#define PIX_FMT_RGB666  fourcc('R', 'G', 'B', '6')  /*< 18  RGB-6-6-6    */
#define PIX_FMT_BGR666  fourcc('B', 'G', 'R', '6')  /*< 18  BGR-6-6-6    */
#define PIX_FMT_BGR24   fourcc('B', 'G', 'R', '3')  /*< 24  BGR-8-8-8    */
#define PIX_FMT_RGB24   fourcc('R', 'G', 'B', '3')  /*< 24  RGB-8-8-8    */

#define PIX_FMT_GREY    fourcc('G', 'R', 'E', 'Y')  /*< 8  Greyscale */

#define FB_SYNC_DATA_ENABLE_HIGH_ACT   (1 << 6)
#define FB_SYNC_DOTCLK_FALLING_ACT     (1 << 7) /* falling/negative edge sampling */

extern int mxsfb_init(struct fb_videomode *mode, uint32_t pixfmt, int bpp);
extern void mxsfb_disable(void);

#endif /* __MXSFB_H */
