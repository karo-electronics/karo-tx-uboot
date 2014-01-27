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

#include <common.h>
#include <errno.h>
#include <lcd.h>
#include <nand.h>
#include <jffs2/load_kernel.h>

#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

static ulong calc_fbsize(void)
{
	return panel_info.vl_row * panel_info.vl_col *
		NBITS(panel_info.vl_bpix) / 8;
}

int karo_load_splashimage(int mode)
{
	int ret;
	unsigned long la = gd->fb_base;
	char *splashimage = getenv("splashimage");
	ulong fbsize = calc_fbsize();
	char *end;

	if (!la || !splashimage)
		return 0;

	if ((simple_strtoul(splashimage, &end, 16) != 0) &&
		*end == '\0') {
		if (mode)
			return 0;
		la = simple_strtoul(splashimage, NULL, 16);
		splashimage = "logo.bmp";
	} else if (!mode) {
		return 0;
	}

	if (tstc())
		return -ENODEV;

	ret = karo_load_part(splashimage, (void *)la, fbsize);
	if (ret) {
		printf("Failed to load logo from '%s': %d\n", splashimage, ret);
		return ret;
	}
	return 0;
}
