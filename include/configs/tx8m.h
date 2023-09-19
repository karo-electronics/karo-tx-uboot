/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#ifndef __TX8M_H
#define __TX8M_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#define CFG_FDTADDR			0x43000000

#define SOC_PREFIX			"imx8m"

#if defined(CONFIG_IMX8MM)
#define SOC_FAMILY			"imx8mm"
#elif defined(CONFIG_IMX8MN)
#define SOC_FAMILY			"imx8mn"
#elif defined(CONFIG_IMX8MP)
#define SOC_FAMILY			"imx8mp"
#else
#error Unsupported SOC type
#endif

#ifdef CONFIG_XPL_BUILD
#if IS_ENABLED(CONFIG_IMX8M_LPDDR4)
#define SPL_DDRFW_SIZE			(2 * (16 + 32) * SZ_1K)
#elif IS_ENABLED(CONFIG_IMX8M_DDR3L)
#define SPL_DDRFW_SIZE			((16 + 32) * SZ_1K)
#else
#error Unsupported SDRAM type
#endif

#if defined(CONFIG_IMX8MM)

#define SPL_OCRAM_SIZE			(0x820000 - 0x7e1000)
#define SPL_DTB_SIZE			(11 * SZ_1K)

/* available OCRAM 252KiB
 * SPL DTB: 8KiB .. ~10KiB
 * DDRFW (DDR3L): (32+16) KiB
 * DDRFW (LPDDR4): 2 * (32+16) KiB
 */

#elif defined(CONFIG_IMX8MN)

#define SPL_OCRAM_SIZE			(0x980000 - 0x900000)
#define SPL_DTB_SIZE			(9 * SZ_1K)

#ifndef CFG_MALLOC_F_ADDR
#define CFG_MALLOC_F_ADDR		(0x00960000 - CONFIG_SPL_SYS_MALLOC_F_LEN)
#endif

#elif defined(CONFIG_IMX8MP)

#define SPL_OCRAM_SIZE			(0x970000 - 0x920000)
#define SPL_DTB_SIZE			(11 * SZ_1K)

#endif /* CONFIG_IMX8MP */
#endif /* CONFIG_XPL_BUILD */

#ifdef CONFIG_ARMV8_SEC_FIRMWARE_SUPPORT
#define CFG_SYS_MEM_RESERVE_SECURE	0
#endif

#define CFG_SYS_INIT_RAM_ADDR		0x40000000
#define CFG_SYS_INIT_RAM_SIZE		SZ_512K

#define CFG_SYS_SDRAM_BASE		0x40000000
#define PHYS_SDRAM			CFG_SYS_SDRAM_BASE

#if defined(CONFIG_KARO_TX8P_ML82)
#define PHYS_SDRAM_1			CFG_SYS_SDRAM_BASE
#define PHYS_SDRAM_1_SIZE		(SZ_4G - SZ_1G)
#define PHYS_SDRAM_2			0x100000000UL
#define PHYS_SDRAM_2_SIZE		SZ_1G
#define PHYS_SDRAM_SIZE			PHYS_SDRAM_1_SIZE
#elif defined(CONFIG_KARO_TX8MM_1620) || defined(CONFIG_KARO_QSXM) || \
	defined(CONFIG_KARO_TX8P) || defined(CONFIG_KARO_TX8MM_1622)
#define PHYS_SDRAM_SIZE			SZ_2G
#elif defined(CONFIG_IMX8MN)
#define PHYS_SDRAM_SIZE			SZ_512M
#elif defined(CONFIG_IMX8MM)
#define PHYS_SDRAM_SIZE			SZ_1G
#else
#error Unsupported Board type
#endif

/* Monitor Command Prompt */

#ifdef CONFIG_KARO_QSXM
#define CFG_SYS_FSL_USDHC_NUM	2
#else /* CONFIG_KARO_QSXM */
#define CFG_SYS_FSL_USDHC_NUM	3
#endif
#define CFG_SYS_FSL_ESDHC_ADDR	0

#ifdef CONFIG_USB_EHCI_MX7
#define CFG_MXC_USB_PORTSC		(PORT_PTS_UTMI | PORT_PTS_PTW)
#endif

#endif /* __TX8M_H */
