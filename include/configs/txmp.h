/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 *
 * Configuration settings for the STM32MP15x CPU
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

/*
 * Configuration of the external SRAM memory used by U-Boot
 */
#define CONFIG_SYS_SDRAM_BASE		STM32_DDR_BASE
#define CONFIG_SYS_INIT_SP_ADDR		CONFIG_SYS_TEXT_BASE
/* required for exception handlers to work */
#define IRAM_BASE_ADDR			(STM32_SYSRAM_BASE + \
					 STM32_SYSRAM_SIZE)

/*
 * Console I/O buffer size
 */
#define CONFIG_SYS_CBSIZE		SZ_2K
#define CONFIG_SYS_MAXARGS		256

/*
 * Needed by "loadb"
 */
#define CONFIG_SYS_LOAD_ADDR		STM32_DDR_BASE

/*
 * Env parameters
 */
#ifndef CONFIG_UBOOT_IGNORE_ENV
#define CONFIG_ENV_CALLBACK_LIST_DEFAULT "baseboard:baseboard"
#endif

/* ATAGs */
#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

/* Extend size of kernel image for uncompression */
#define CONFIG_SYS_BOOTM_LEN		SZ_32M

/* SPL support */
#ifdef CONFIG_SPL
/* SPL use DDR */
#define CONFIG_SPL_BSS_START_ADDR	0xC0200000
#define CONFIG_SPL_BSS_MAX_SIZE		0x00100000
#define CONFIG_SYS_SPL_MALLOC_START	0xC0300000
#define CONFIG_SYS_SPL_MALLOC_SIZE	0x00100000

/* limit SYSRAM usage to first 128 KB */
#define CONFIG_SPL_MAX_SIZE		0x00020000
#define CONFIG_SPL_STACK		IRAM_BASE_ADDR
#endif /* CONFIG_SPL */

/* SDMMC */
#define CONFIG_SYS_MMC_MAX_DEVICE	3
#define CONFIG_SUPPORT_EMMC_BOOT

/*****************************************************************************/

/* Ethernet */
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY	(1 * SZ_1M)	/* 1M */
#endif
#define CONFIG_SYS_AUTOLOAD		"no"

#ifdef CONFIG_DM_VIDEO
#define CONFIG_VIDEO_LOGO
#define CONFIG_VIDEO_BMP_LOGO
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP
#endif

#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define BOOT_PART_STR		__stringify(CONFIG_SYS_MMCSD_FS_BOOT_PARTITION)

#if IS_ENABLED(CONFIG_EFI_PARTITION)
/* GPT requires 64bit sector numbers */
#define CONFIG_SYS_64BIT_LBA
#endif

#ifndef CONFIG_SPL_BUILD
#ifdef CONFIG_TXMP_NOR
#define TXMP_NOR_MTDPARTS \
	"mtdparts_nor0=256k(fsbl1),1m(ssbl),256k(logo),256k(dtb),-(nor_user)\0"
#else
#define TXMP_NOR_MTDPARTS		""
#endif

#ifdef CONFIG_TXMP_NAND
#define TXMP_NAND_MTDPARTS \
	"mtdparts_nand0=512k(fsbl),2m(ssbl),8m(linux),32m(rootfs),86528k(userfs),512k(dtb),512k@0x7f80000(bbt)ro\0"
#else
#define TXMP_NAND_MTDPARTS		""
#endif
#define VIDEOMODE_DEFAULT		"videomode=ETV570\0"

#define FDT_ADDR_STR			"c4000000"

#if defined(CONFIG_KARO_TXMP_1530)
#define TXMP_BASEBOARD			"mb7"
#elif defined(CONFIG_KARO_TXMP_1570)
#define TXMP_BASEBOARD			"mb7"
#elif defined(CONFIG_KARO_QSMP_1510)
#define TXMP_BASEBOARD			"qsbase1"
#elif defined(CONFIG_KARO_QSMP_1530)
#define TXMP_BASEBOARD			"qsbase1"
#elif defined(CONFIG_KARO_QSMP_1570)
#define TXMP_BASEBOARD			"qsbase1"
#else
#define TXMP_BASEBOARD			""
#endif

#ifndef CONFIG_USE_DEFAULT_ENV_FILE
#ifdef CONFIG_UBOOT_IGNORE_ENV
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autoload=no\0"							\
	"autostart=no\0"						\
	"bootdelay=-1\0"						\
	"boot_mode=mmc\0"						\
	"fdtaddr=" FDT_ADDR_STR "\0"					\
	"fdt_addr_r=" FDT_ADDR_STR "\0"					\
	"fdt_high=ffffffff\0"						\
	TXMP_NAND_MTDPARTS						\
	TXMP_NOR_MTDPARTS
#else
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autoload=no\0"							\
	"autostart=no\0"						\
	"baseboard=" TXMP_BASEBOARD "\0"				\
	"bootargs_mmc=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=PARTUUID=${uuid_rootfs} rootwait ${append_bootargs}\0"	\
	"bootargs_nfs=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=/dev/nfs nfsroot=${nfs_server}:${nfsroot},nolock"	\
	" ip=dhcp ${append_bootargs}\0"					\
	"bootcmd_mmc=setenv autostart no;run bootargs_mmc"		\
	";load ${bootdev} ${bootpart} ${loadaddr} uImage"		\
	";load ${bootdev} ${bootpart} ${fdt_addr_r} ${dtbfile}\0"	\
	"bootcmd_net=setenv autoload y;setenv autostart n"		\
	";run bootargs_nfs;dhcp\0"					\
	"bootdev=mmc\0"							\
	"bootlimit=0\0"							\
	"bootm_cmd=bootm ${loadaddr} ${ramdisk_addr_r} ${fdt_addr_r}\0"	\
	"bootpart=0:2\0"						\
	"boot_mode=mmc\0"						\
	"boot_net_usb_start=true\0"					\
	"default_bootargs=setenv bootargs init=/linuxrc"		\
	" console=ttySTM0,115200 ro debug panic=0\0"			\
	"dtbfile=/" CONFIG_DEFAULT_DEVICE_TREE ".dtb\0"			\
	"emmc_boot_ack=1\0"						\
	"emmc_boot_part=" BOOT_PART_STR "\0"				\
	"fdtaddr=" FDT_ADDR_STR "\0"					\
	"fdtsave=save ${bootdev} ${bootpart}"				\
	" ${fdt_addr_r} ${dtbfile} ${fdtsize}\0"			\
	"fdt_addr_r=" FDT_ADDR_STR "\0"					\
	"fdt_high=ffffffff\0"						\
	"initrd_high=ffffffff\0"					\
	"loadaddr=c0000000\0"						\
	"kernel_addr_r=c2000000\0"					\
	"ramdisk_addr_r=-\0"						\
	"usb_pgood_delay=2000\0"					\
	VIDEOMODE_DEFAULT						\
	TXMP_NAND_MTDPARTS						\
	TXMP_NOR_MTDPARTS
#endif /* CONFIG_UBOOT_IGNORE_ENV */
#endif /* CONFIG_USE_DEFAULT_ENV_FILE */
#endif /* CONFIG_SPL_BUILD */

#endif /* __CONFIG_H */
