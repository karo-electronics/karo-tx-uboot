/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright Marcel Mertens <mm@karo-electronics.de>
 *
 * based on: imx91_evk.h Copyright 2024 NXP
 */

#ifndef __KARO_TX91_H
#define __KARO_TX91_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#define SOC_PREFIX			"imx91"
#define SOC_FAMILY			"imx91"

#define CFG_SYS_UBOOT_BASE	\
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#define CFG_FDTADDR			0x83000000

#define CFG_SYS_INIT_RAM_ADDR		0x80000000
#define CFG_SYS_INIT_RAM_SIZE		SZ_512K

#define CFG_SYS_SDRAM_BASE		0x80000000
#define PHYS_SDRAM			CFG_SYS_SDRAM_BASE
#if IS_ENABLED(CONFIG_IMX9_DRAM_INLINE_ECC)
#define PHYS_SDRAM_SIZE			(SZ_512M - SZ_64M)
#else
#define PHYS_SDRAM_SIZE			SZ_512M
#endif

#define SPL_DDRFW_SIZE			(2 * (SZ_32K + SZ_16K))

#if CONFIG_IS_ENABLED(SYS_MALLOC_F) && !defined(CFG_MALLOC_F_ADDR)
//#define CFG_MALLOC_F_ADDR		0x20499e00
#endif

#define CFG_SYS_FSL_USDHC_NUM		2

/* Using ULP WDOG for reset */
#define WDOG_BASE_ADDR			WDG3_BASE_ADDR

#endif /* __KARO_TX91_H */
