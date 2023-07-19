/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Lothar Waßmann <LW@KARO-electronics.de>
 */

#ifndef __KARO_TX93_H
#define __KARO_TX93_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#define SOC_PREFIX			"imx93"
#define SOC_FAMILY			"imx93"

#define CFG_SYS_UBOOT_BASE	\
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#define CFG_FDTADDR			0x83000000

/* Link Definitions */

#define CFG_SYS_INIT_RAM_ADDR		0x80000000
#define CFG_SYS_INIT_RAM_SIZE		SZ_512K

#define CFG_SYS_SDRAM_BASE		0x80000000
#define PHYS_SDRAM			CFG_SYS_SDRAM_BASE
#define PHYS_SDRAM_SIZE			SZ_1G

#define CFG_SYS_FSL_USDHC_NUM		2

#define SPL_DDRFW_SIZE			(2 * (SZ_32K + SZ_16K))

/* Using ULP WDOG for reset */
#define WDOG_BASE_ADDR			WDG3_BASE_ADDR

#endif /* __KARO_TX93_H */
