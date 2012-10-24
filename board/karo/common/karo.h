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

void karo_fdt_remove_node(void *blob, const char *node);
void karo_fdt_move_fdt(void);
void karo_fdt_fixup_touchpanel(void *blob);
void karo_fdt_fixup_usb_otg(void *blob);
void karo_fdt_del_prop(void *blob, const char *compat, phys_addr_t offs,
		const char *prop);
void *karo_fdt_load_dtb(void);

int karo_load_splashimage(int mode);
