/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Lothar Waßmann <LW@KARO-electronics.de>
 */

#ifndef __KARO_TX93_H
#define __KARO_TX93_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

//#define __pfx(p, v)			(p##v)
//#define _pfx(p, v)			__pfx(p, v)

#define CONFIG_SYS_BOOTM_LEN		SZ_64M
#define CONFIG_SPL_MAX_SIZE		(148 * 1024)
#define CONFIG_SYS_MONITOR_LEN		SZ_512K
#define CONFIG_SYS_UBOOT_BASE	\
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#define CONFIG_FDTADDR			0x83000000

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_STACK		0x20519dd0
#define CONFIG_SPL_BSS_START_ADDR	0x2051a000
#define CONFIG_SPL_BSS_MAX_SIZE		SZ_8K	/* 8 KB */
#define CONFIG_SYS_SPL_MALLOC_START	0x83200000 /* Need disable simple malloc where still uses m+alloc_f area */
#define CONFIG_SYS_SPL_MALLOC_SIZE	SZ_512K	/* 512 KB */

/* For RAW image gives a error info not panic */
#define CONFIG_SPL_ABORT_ON_RAW_IMAGE

#endif

/* Link Definitions */

#define CONFIG_SYS_INIT_RAM_ADDR	0x80000000
#define CONFIG_SYS_INIT_RAM_SIZE	0x200000
#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define CONFIG_SYS_SDRAM_BASE		0x80000000
#define PHYS_SDRAM			0x80000000
#define PHYS_SDRAM_SIZE			SZ_1G

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)

#define CONFIG_SYS_FSL_USDHC_NUM	2

/* Using ULP WDOG for reset */
#define WDOG_BASE_ADDR			WDG3_BASE_ADDR

#define CONFIG_SYS_I2C_SPEED		100000

/* USB configs */
#define CONFIG_USB_MAX_CONTROLLER_COUNT	2

#define CONFIG_ETHPRIME		"eth0"

#endif /* __KARO_TX93_H */
