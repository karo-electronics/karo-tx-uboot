/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2026 Ka-Ro electronics GmbH
 *
 * Ka-Ro i.MX6 modules: what Kconfig and DT cannot achieve.
 */

#ifndef __IMX6_KARO_CONFIG_H
#define __IMX6_KARO_CONFIG_H

#include <asm/arch/imx-regs.h>
#include <linux/sizes.h>
#include "mx6_common.h"

/* Console: UART1 = TXCOM "1st UART", read by serial_mxc.c. */
#define CFG_MXC_UART_BASE	UART1_BASE

/* DRAM base per SoC; size probed at runtime. */
#define PHYS_SDRAM		MMDC0_ARB_BASE_ADDR
#define CFG_SYS_SDRAM_BASE	PHYS_SDRAM

/* OCRAM early stack; system-constants.h derives SYS_INIT_SP_ADDR. */
#define CFG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CFG_SYS_INIT_RAM_SIZE	IRAM_SIZE

/* eMMC on USDHC2; read by mach-imx/speed.c get_clocks(). */
#define CFG_SYS_FSL_ESDHC_ADDR	USDHC2_BASE_ADDR

/* mxsfb.c hardcodes MXS_LCDIF_BASE; arch-mx6 lacks it. Unused on MX6S. */
#define MXS_LCDIF_BASE		MX6ULL_LCDIF1_BASE_ADDR

/* Must equal ldb_di0/7 as programmed by tx6_setup_ldb(); LVDS only. */
#define CFG_SYS_LDB_CLOCK	71000000

/* CFG_FDTADDR, SOC_PREFIX, SOC_FAMILY: read by karo_* helpers. */
#if defined(CONFIG_MX6S) || defined(CONFIG_MX6DL)
#define CFG_FDTADDR		0x13000000
#define SOC_PREFIX		"imx6dl"
#define SOC_FAMILY		"imx6dl"
#elif defined(CONFIG_MX6Q)
#define CFG_FDTADDR		0x13000000
#define SOC_PREFIX		"imx6q"
#define SOC_FAMILY		"imx6q"
#elif defined(CONFIG_MX6ULL)
#define CFG_FDTADDR		0x83000000
#define SOC_PREFIX		"imx6ul"
#define SOC_FAMILY		"imx6ull"
#elif defined(CONFIG_MX6UL)
#define CFG_FDTADDR		0x83000000
#define SOC_PREFIX		"imx6ul"
#define SOC_FAMILY		"imx6ul"
#else
#error Unsupported i.MX6 variant
#endif

#endif /* __IMX6_KARO_CONFIG_H */
