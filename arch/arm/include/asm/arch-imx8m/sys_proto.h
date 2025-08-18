/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2017 NXP
 */

#ifndef __ARCH_IMX8M_SYS_PROTO_H
#define __ARCH_IMX8M_SYS_PROTO_H

#include <asm/mach-imx/sys_proto.h>
#include <asm/arch/imx-regs.h>

void set_wdog_reset(struct wdog_regs *wdog);
void enable_tzc380(void);
void restore_boot_params(void);
extern unsigned long rom_pointer[];

static inline bool is_usb_boot(void)
{
	return get_boot_device() == USB_BOOT;
}

#define is_boot_from_usb()	is_usb_boot()

#endif
