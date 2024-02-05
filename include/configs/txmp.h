/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (C) 2023 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * Configuration settings for the STM32MP15x/STM32MP13X CPU
 */

#ifndef __CONFIG_H
#define __CONFIG_H
#include <linux/sizes.h>
#include <asm/arch/stm32.h>

#ifndef CONFIG_TFABOOT
/* PSCI support */
#define CONFIG_ARMV7_SECURE_BASE	STM32_SYSRAM_BASE
#define CONFIG_ARMV7_SECURE_MAX_SIZE	STM32_SYSRAM_SIZE
#endif

#ifndef CONFIG_SPL_BUILD
#define CONFIG_SHOW_ACTIVITY
#endif

#if defined(CONFIG_STM32MP13X)
#define SOC_PREFIX "stm32mp13"
#elif defined(CONFIG_STM32MP15X)
#define SOC_PREFIX "stm32mp15"
#else
#error Unsupported STM32MP1 SoC
#endif

/*
 * Configuration of the external SRAM memory used by U-Boot
 */
#define CONFIG_SYS_SDRAM_BASE		STM32_DDR_BASE
/* required for exception handlers to work */
#define IRAM_BASE_ADDR			(STM32_SYSRAM_BASE + \
					 STM32_SYSRAM_SIZE)

/* ATAGs */
#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

/* SDMMC */

/*****************************************************************************/

/* Ethernet */
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY	(1 * SZ_1M)	/* 1M */
#endif

#if defined(CONFIG_KARO_QSMP_1351)
#define SOC_FAMILY "stm32mp135c"
#elif defined(CONFIG_KARO_QSMP_1510)
#define SOC_FAMILY "stm32mp151a"
#elif defined(CONFIG_KARO_QSMP_1530)
#define SOC_FAMILY "stm32mp153a"
#elif defined(CONFIG_KARO_QSMP_1570)
#define SOC_FAMILY "stm32mp157c"
#elif defined(CONFIG_KARO_TXMP_1570)
#define SOC_FAMILY "stm32mp157c"
#elif defined(CONFIG_KARO_TXMP_1571)
#define SOC_FAMILY "stm32mp157c"
#else
#error Unsupported Ka-Ro STM32MP1 module
#endif

#endif /* __CONFIG_H */
