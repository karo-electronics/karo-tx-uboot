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
#if CONFIG_IS_ENABLED(LED)
#define CONFIG_SHOW_ACTIVITY
#endif
#endif

#if defined(CONFIG_STM32MP13X)
#define SOC_PREFIX "stm32mp13"
#define CONFIG_SYS_NONCACHED_MEMORY	SZ_1M
#elif defined(CONFIG_STM32MP15X)
#define SOC_PREFIX "stm32mp15"
#elif defined(CONFIG_STM32MP23X)
#define SOC_PREFIX "stm32mp23"
#elif defined(CONFIG_STM32MP25X)
#define SOC_PREFIX "stm32mp25"
#else
#error Unsupported STM32MP1 SoC
#endif

#if defined(CONFIG_KARO_QSMP_1351)
#define SOC_FAMILY "stm32mp135c"
#elif defined(CONFIG_KARO_QSMP_1530)
#define SOC_FAMILY "stm32mp153a"
#elif defined(CONFIG_KARO_QSMP_1570)
#define SOC_FAMILY "stm32mp157c"
#elif defined(CONFIG_KARO_TXMP_1570)
#define SOC_FAMILY "stm32mp157c"
#elif defined(CONFIG_KARO_TXMP_1571)
#define SOC_FAMILY "stm32mp157c"
#elif defined(CONFIG_KARO_QSMP_2350)
#define SOC_FAMILY "stm32mp235c"
#elif defined(CONFIG_KARO_QSMP_2550)
#define SOC_FAMILY "stm32mp255f"
#elif defined(CONFIG_KARO_TXMP_2550)
#define SOC_FAMILY "stm32mp257f"
#elif defined(CONFIG_KARO_TXMP_2570)
#define SOC_FAMILY "stm32mp2557f"
#else
#error Unsupported Ka-Ro STM32MP module
#endif

/*
 * Configuration of the external SRAM memory used by U-Boot
 */
#define PHYS_SDRAM_1			STM32_DDR_BASE
#define CONFIG_SYS_SDRAM_BASE		STM32_DDR_BASE

/* required for exception handlers to work */
#define IRAM_BASE_ADDR			(STM32_SYSRAM_BASE + \
					 STM32_SYSRAM_SIZE)

/* ATAGs */
#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

/*****************************************************************************/

#endif /* __CONFIG_H */
