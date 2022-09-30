/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (C) 2021, STMicroelectronics - All Rights Reserved
 *
 * Configuration settings for the STM32MP15x CPU
 */

#ifndef __CONFIG_STM32MP13_COMMMON_H
#define __CONFIG_STM32MP13_COMMMON_H
#include <linux/sizes.h>
#include <asm/arch/stm32.h>

/*
 * Configuration of the external SRAM memory used by U-Boot
 */
#define CONFIG_SYS_SDRAM_BASE		STM32_DDR_BASE
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_TEXT_BASE + SZ_4M)

/*
 * Console I/O buffer size
 */
#define CONFIG_SYS_CBSIZE		SZ_1K

/*
 * default load address used for command tftp,  bootm , loadb, ...
 */
#define CONFIG_LOADADDR			0xc2000000
#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR

/*
 * For booting Linux, use the first 256 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CONFIG_SYS_BOOTMAPSZ		SZ_256M

/* Extend size of kernel image for uncompression */
#define CONFIG_SYS_BOOTM_LEN		SZ_32M

/*MMC SD*/
#define CONFIG_SYS_MMC_MAX_DEVICE	2

/* NAND support */
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_MAX_NAND_DEVICE	1

/* Ethernet need */
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY	(1 * SZ_1M)
#define CONFIG_SERVERIP                 192.168.1.1
#define CONFIG_BOOTP_SERVERIP
#define CONFIG_SYS_AUTOLOAD		"no"
#endif

/*****************************************************************************/
#ifdef CONFIG_DISTRO_DEFAULTS
/*****************************************************************************/

#ifdef CONFIG_CMD_MMC
#define BOOT_TARGET_MMC0(func)	func(MMC, mmc, 0)
#define BOOT_TARGET_MMC1(func)	func(MMC, mmc, 1)
#else
#define BOOT_TARGET_MMC0(func)
#define BOOT_TARGET_MMC1(func)
#endif

#ifdef CONFIG_NET
#define BOOT_TARGET_PXE(func)	func(PXE, pxe, na)
#else
#define BOOT_TARGET_PXE(func)
#endif

#ifdef CONFIG_CMD_UBIFS
#define BOOT_TARGET_UBIFS(func)	func(UBIFS, ubifs, 0)
#else
#define BOOT_TARGET_UBIFS(func)
#endif

#ifdef CONFIG_CMD_USB
#define BOOT_TARGET_USB(func)	func(USB, usb, 0)
#else
#define BOOT_TARGET_USB(func)
#endif

#define BOOT_TARGET_DEVICES(func)	\
	BOOT_TARGET_MMC1(func)		\
	BOOT_TARGET_UBIFS(func)		\
	BOOT_TARGET_MMC0(func)		\
	BOOT_TARGET_USB(func)		\
	BOOT_TARGET_PXE(func)

/*
 * default bootcmd for stm32mp13:
 * for serial/usb: execute the stm32prog command
 * for mmc boot (eMMC, SD card), distro boot on the same mmc device
 * for nand or spi-nand boot, distro boot with ubifs on UBI partition
 * for nor boot, use the default distro order in ${boot_targets}
 */
#define STM32MP_BOOTCMD "bootcmd_stm32mp=" \
	"echo \"Boot over ${boot_device}${boot_instance}!\";" \
	"if test ${boot_device} = serial || test ${boot_device} = usb;" \
	"then stm32prog ${boot_device} ${boot_instance}; " \
	"else " \
		"run env_check;" \
		"if test ${boot_device} = mmc;" \
		"then env set boot_targets \"mmc${boot_instance}\"; fi;" \
		"if test ${boot_device} = nand ||" \
		  " test ${boot_device} = spi-nand ;" \
		"then env set boot_targets ubifs0; fi;" \
		"run distro_bootcmd;" \
	"fi;\0"

#define STM32MP_EXTRA \
	"env_check=if env info -p -d -q; then env save; fi\0" \
	"boot_net_usb_start=true\0"

#ifndef STM32MP_BOARD_EXTRA_ENV
#define STM32MP_BOARD_EXTRA_ENV
#endif

#include <config_distro_bootcmd.h>

/*
 * memory layout for 32M uncompressed/compressed kernel,
 * 1M fdt, 1M script, 1M pxe and 1M for overlay
 * and the ramdisk at the end.
 */
#define __KERNEL_ADDR_R     __stringify(0xc2000000)
#define __FDT_ADDR_R        __stringify(0xc4000000)
#define __SCRIPT_ADDR_R     __stringify(0xc4100000)
#define __PXEFILE_ADDR_R    __stringify(0xc4200000)
#define __FDTOVERLAY_ADDR_R __stringify(0xc4300000)
#define __RAMDISK_ADDR_R    __stringify(0xc4400000)

#define STM32MP_MEM_LAYOUT \
	"kernel_addr_r=" __KERNEL_ADDR_R "\0" \
	"fdt_addr_r=" __FDT_ADDR_R "\0" \
	"scriptaddr=" __SCRIPT_ADDR_R "\0" \
	"pxefile_addr_r=" __PXEFILE_ADDR_R "\0" \
	"fdtoverlay_addr_r=" __FDTOVERLAY_ADDR_R "\0" \
	"ramdisk_addr_r=" __RAMDISK_ADDR_R "\0"

#define CONFIG_EXTRA_ENV_SETTINGS \
	STM32MP_MEM_LAYOUT \
	STM32MP_BOOTCMD \
	BOOTENV \
	STM32MP_EXTRA \
	STM32MP_BOARD_EXTRA_ENV

#endif /* ifdef CONFIG_DISTRO_DEFAULTS*/

#endif /* __CONFIG_STM32MP13_COMMMON_H */
