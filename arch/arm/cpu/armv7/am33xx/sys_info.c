/*
 * sys_info.c
 *
 * System information functions
 *
 * Copyright (C) 2011, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * Derived from Beagle Board and 3430 SDP code by
 *      Richard Woodruff <r-woodruff2@ti.com>
 *      Syed Mohammed Khasim <khasim@ti.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>

struct ctrl_stat *cstat = (struct ctrl_stat *)CTRL_BASE;

/**
 * get_cpu_rev(void) - extract rev info
 */
u32 get_cpu_rev(void)
{
	u32 id;
	u32 rev;

	id = readl(DEVICE_ID);
	rev = (id >> 28) & 0xff;

	return rev;
}

/**
 * get_cpu_type(void) - extract cpu info
 */
u32 get_cpu_type(void)
{
	u32 id = 0;
	u32 partnum;

	id = readl(DEVICE_ID);
	partnum = (id >> 12) & 0xffff;

	return partnum;
}

/**
 * get_board_rev() - setup to pass kernel board revision information
 * returns:(bit[0-3] sub version, higher bit[7-4] is higher version)
 */
u32 get_board_rev(void)
{
	return BOARD_REV_ID;
}

/**
 * get_device_type(): tell if GP/HS/EMU/TST
 */
u32 get_device_type(void)
{
	int mode;
	mode = readl(&cstat->statusreg) & DEVICE_MASK;
	return mode >>= 8;
}

/**
 * get_sysboot_value(void) - return SYS_BOOT[4:0]
 */
u32 get_sysboot_value(void)
{
	int mode;
	mode = readl(&cstat->statusreg) & SYSBOOT_MASK;
	return mode;
}

#ifdef CONFIG_DISPLAY_CPUINFO
static char *cpu_revs[] = {
	"1.0",
	"2.0",
	"2.1",
};

static char *dev_types[] = {
	"TST",
	"EMU",
	"HS",
	"GP",
};

/**
 * Print CPU information
 */
int print_cpuinfo(void)
{
	char *cpu_s, *sec_s, *rev_s;

	switch (get_cpu_type()) {
	case AM335X:
		cpu_s = "AM335X";
		break;
	case TI81XX:
		cpu_s = "TI81XX";
		break;
	default:
		cpu_s = "Unknown CPU type";
	}

	if (get_cpu_rev() < ARRAY_SIZE(cpu_revs))
		rev_s = cpu_revs[get_cpu_rev()];
	else
		rev_s = "?";

	if (get_device_type() < ARRAY_SIZE(dev_types))
		sec_s = dev_types[get_device_type()];
	else
		sec_s = "?";

	printf("%s-%s rev %s\n", cpu_s, sec_s, rev_s);

	return 0;
}
#endif	/* CONFIG_DISPLAY_CPUINFO */
