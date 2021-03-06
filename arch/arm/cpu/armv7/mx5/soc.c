/*
 * (C) Copyright 2007
 * Sascha Hauer, Pengutronix
 *
 * (C) Copyright 2009 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>

#include <asm/errno.h>
#include <asm/io.h>
#include <asm/imx-common/boot_mode.h>

#if !(defined(CONFIG_SOC_MX51) || defined(CONFIG_SOC_MX53))
#error "CPU_TYPE not defined"
#endif

#ifdef CONFIG_HW_WATCHDOG
#define wdog_base	((void *)WDOG1_BASE_ADDR)
#define WDOG_WCR	0x00
#define WCR_WDE		(1 << 2)
#define WDOG_WSR	0x02

void hw_watchdog_reset(void)
{
	if (readw(wdog_base + WDOG_WCR) & WCR_WDE) {
		static u16 toggle = 0xaaaa;

		writew(toggle, wdog_base + WDOG_WSR);
		toggle ^= 0xffff;
	}
}
#endif

u32 get_cpu_rev(void)
{
#ifdef CONFIG_SOC_MX51
	int system_rev = 0x51000;
#else
	int system_rev = 0x53000;
#endif
	int reg = __raw_readl(ROM_SI_REV);

#if defined(CONFIG_SOC_MX51)
	switch (reg) {
	case 0x02:
		system_rev |= CHIP_REV_1_1;
		break;
	case 0x10:
		if ((__raw_readl(GPIO1_BASE_ADDR + 0x0) & (0x1 << 22)) == 0)
			system_rev |= CHIP_REV_2_5;
		else
			system_rev |= CHIP_REV_2_0;
		break;
	case 0x20:
		system_rev |= CHIP_REV_3_0;
		break;
	default:
		system_rev |= CHIP_REV_1_0;
		break;
	}
#else
	if (reg < 0x20)
		system_rev |= CHIP_REV_1_0;
	else
		system_rev |= reg;
#endif
	return system_rev;
}

#ifdef CONFIG_REVISION_TAG
u32 __weak get_board_rev(void)
{
	return get_cpu_rev();
}
#endif

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in start.S */
	dcache_enable();
}
#endif

#if defined(CONFIG_FEC_MXC)
static void __imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	int i;
	struct iim_regs *iim = (struct iim_regs *)IMX_IIM_BASE;
	struct fuse_bank *bank = &iim->bank[1];
	struct fuse_bank1_regs *fuse =
			(struct fuse_bank1_regs *)bank->fuse_regs;

	for (i = 0; i < 6; i++)
		mac[i] = readl(&fuse->mac_addr[i]) & 0xff;
}

void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
	__attribute__((weak, alias("__imx_get_mac_from_fuse")));

#endif

#ifdef CONFIG_SOC_MX53
void boot_mode_apply(unsigned cfg_val)
{
	writel(cfg_val, &((struct srtc_regs *)SRTC_BASE_ADDR)->lpgr);
}
/*
 * cfg_val will be used for
 * Boot_cfg3[7:0]:Boot_cfg2[7:0]:Boot_cfg1[7:0]
 *
 * If bit 28 of LPGR is set upon watchdog reset,
 * bits[25:0] of LPGR will move to SBMR.
 */
const struct boot_mode soc_boot_modes[] = {
	{"normal",	MAKE_CFGVAL(0x00, 0x00, 0x00, 0x00)},
	/* usb or serial download */
	{"usb",		MAKE_CFGVAL(0x00, 0x00, 0x00, 0x13)},
	{"sata",	MAKE_CFGVAL(0x28, 0x00, 0x00, 0x12)},
	{"escpi1:0",	MAKE_CFGVAL(0x38, 0x20, 0x00, 0x12)},
	{"escpi1:1",	MAKE_CFGVAL(0x38, 0x20, 0x04, 0x12)},
	{"escpi1:2",	MAKE_CFGVAL(0x38, 0x20, 0x08, 0x12)},
	{"escpi1:3",	MAKE_CFGVAL(0x38, 0x20, 0x0c, 0x12)},
	/* 4 bit bus width */
	{"esdhc1",	MAKE_CFGVAL(0x40, 0x20, 0x00, 0x12)},
	{"esdhc2",	MAKE_CFGVAL(0x40, 0x20, 0x08, 0x12)},
	{"esdhc3",	MAKE_CFGVAL(0x40, 0x20, 0x10, 0x12)},
	{"esdhc4",	MAKE_CFGVAL(0x40, 0x20, 0x18, 0x12)},
	{NULL,		0},
};
#endif
