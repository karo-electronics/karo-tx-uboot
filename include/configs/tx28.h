/*
 * Copyright (C) 2012 <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:      GPL-2.0
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/kconfig.h>
#include <linux/sizes.h>
#include <asm/arch/regs-base.h>

/*
 * Ka-Ro TX28 board - SoC configuration
 */
#define CONFIG_MXS_GPIO					/* GPIO control */
#define CONFIG_SYS_HZ			1000		/* Ticks per second */
#define PHYS_SDRAM_1_SIZE		CONFIG_SYS_SDRAM_SIZE
#ifdef CONFIG_TX28_S
#define TX28_MOD_SUFFIX			"1"
#else
#define CONFIG_SYS_SPL_FIXED_BATT_SUPPLY
#define TX28_MOD_SUFFIX			"0"
#endif
#define IRAM_BASE_ADDR			0x00000000

#ifndef CONFIG_SPL_BUILD
#define CONFIG_SKIP_LOWLEVEL_INIT
#define CONFIG_SHOW_ACTIVITY
#define CONFIG_ARCH_CPU_INIT
#define CONFIG_ARCH_MISC_INIT		/* init vector table after relocation */
#define CONFIG_DISPLAY_CPUINFO
#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_SYS_GENERIC_BOARD

/* LCD Logo and Splash screen support */
#ifdef CONFIG_LCD
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_VIDEO_MXS
#define CONFIG_LCD_LOGO
#define LCD_BPP				LCD_COLOR32
#define CONFIG_CMD_BMP
#define CONFIG_VIDEO_BMP_RLE8
#endif /* CONFIG_LCD */
#endif /* CONFIG_SPL_BUILD */

/*
 * Memory configuration options
 */
#define CONFIG_NR_DRAM_BANKS		0x1		/* 1 bank of SDRAM */
#define PHYS_SDRAM_1			0x40000000	/* SDRAM Bank #1 */
#define CONFIG_STACKSIZE		SZ_64K
#define CONFIG_SYS_MALLOC_LEN		SZ_4M
#define CONFIG_SYS_MEMTEST_START	PHYS_SDRAM_1	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + SZ_4M)

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_CBSIZE		2048		/* Console I/O buffer size */
#define CONFIG_SYS_PBSIZE \
	(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
							/* Print buffer size */
#define CONFIG_SYS_MAXARGS		256		/* Max number of command args */
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
					/* Boot argument buffer size */
#define CONFIG_VERSION_VARIABLE		/* U-BOOT version */
#define CONFIG_AUTO_COMPLETE		/* Command auto complete */
#define CONFIG_CMDLINE_EDITING		/* Command history etc */

#define CONFIG_SYS_64BIT_VSPRINTF

/*
 * Flattened Device Tree (FDT) support
*/
#ifdef CONFIG_OF_LIBFDT
#endif

/*
 * Boot Linux
 */
#define xstr(s)				str(s)
#define str(s)				#s
#define __pfx(x, s)			(x##s)
#define _pfx(x, s)			__pfx(x, s)

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_BOOTDELAY		3
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_SYS_AUTOLOAD		"no"
#define CONFIG_BOOTFILE			"uImage"
#define CONFIG_BOOTARGS			"init=/linuxrc console=ttyAMA0,115200 ro debug panic=1"
#define CONFIG_BOOTCOMMAND		"run bootcmd_${boot_mode} bootm_cmd"
#ifdef CONFIG_TX28_S
#define CONFIG_LOADADDR			41000000
#else
#define CONFIG_LOADADDR			43000000
#endif
#define CONFIG_FDTADDR			40800000
#define CONFIG_SYS_LOAD_ADDR		_pfx(0x, CONFIG_LOADADDR)
#define CONFIG_SYS_FDT_ADDR		_pfx(0x, CONFIG_FDTADDR)
#define CONFIG_U_BOOT_IMG_SIZE		SZ_1M

/*
 * Extra Environment Settings
 */
#ifdef CONFIG_TX28_UBOOT_NOENV
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"autoload=no\0"							\
	"baseboard=stk5-v3\0"						\
	"bootdelay=-1\0"						\
	"fdtaddr=" xstr(CONFIG_FDTADDR) "\0"				\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"
#else
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"baseboard=stk5-v3\0"						\
	"bootargs_jffs2=run default_bootargs"				\
	";setenv bootargs ${bootargs}"					\
	" root=/dev/mtdblock3 rootfstype=jffs2\0"			\
	"bootargs_mmc=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=/dev/mmcblk0p3 rootwait\0"				\
	"bootargs_nfs=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=/dev/nfs nfsroot=${nfs_server}:${nfsroot},nolock"	\
	" ip=dhcp\0"							\
	"bootargs_ubifs=run default_bootargs"				\
	";setenv bootargs ${bootargs}"					\
	" ubi.mtd=rootfs root=ubi0:rootfs rootfstype=ubifs\0"		\
	"bootcmd_jffs2=setenv autostart no;run bootargs_jffs2"		\
	";nboot linux\0"						\
	"bootcmd_mmc=setenv autostart no;run bootargs_mmc"		\
	";fatload mmc 0 ${loadaddr} uImage\0"				\
	"bootcmd_nand=setenv autostart no;run bootargs_ubifs"		\
	";nboot linux\0"						\
	"bootcmd_net=setenv autoload y;setenv autostart n"		\
	";run bootargs_nfs"						\
	";dhcp\0"							\
	"bootm_cmd=bootm ${loadaddr} - ${fdtaddr}\0"			\
	"boot_mode=nand\0"						\
	"default_bootargs=setenv bootargs " CONFIG_BOOTARGS		\
	" ${append_bootargs}\0"						\
	"fdtaddr=" xstr(CONFIG_FDTADDR) "\0"				\
	"fdtsave=fdt resize;nand erase.part dtb"			\
	";nand write ${fdtaddr} dtb ${fdtsize}\0"			\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"				\
	"nfsroot=/tftpboot/rootfs\0"					\
	"otg_mode=device\0"						\
	"touchpanel=tsc2007\0"						\
	"video_mode=VGA\0"
#endif /*  CONFIG_ENV_IS_NOWHERE */

#define MTD_NAME			"gpmi-nand"
#define MTDIDS_DEFAULT			"nand0=" MTD_NAME

/*
 * Serial Driver
 */
#define CONFIG_PL011_SERIAL
#define CONFIG_PL011_CLOCK		24000000
#define CONFIG_PL01x_PORTS	{	\
	(void *)MXS_UARTDBG_BASE,	\
	}
#define CONFIG_CONS_INDEX		0		/* do not change! */
#define CONFIG_BAUDRATE			115200		/* Default baud rate */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200, }
#define CONFIG_SYS_CONSOLE_INFO_QUIET

/*
 * Ethernet Driver
 */
#ifdef CONFIG_FEC_MXC
/* This is required for the FEC driver to work with cache enabled */
#define CONFIG_SYS_ARM_CACHE_WRITETHROUGH
#define CONFIG_SYS_CACHELINE_SIZE	32

#ifdef CONFIG_FEC_MXC_PHYADDR
#define IMX_FEC_BASE			MXS_ENET0_BASE
#endif

#define CONFIG_FEC_XCV_TYPE		RMII
#endif

/*
 * NAND flash driver
 */
#ifdef CONFIG_NAND
#define CONFIG_SYS_NAND_BLOCK_SIZE	SZ_128K
#define CONFIG_SYS_NAND_U_BOOT_OFFS	CONFIG_SYS_NAND_BLOCK_SIZE
#define CONFIG_SYS_MXS_DMA_CHANNEL	4
#define CONFIG_SYS_NAND_MAX_CHIPS	0x1
#define CONFIG_SYS_MAX_NAND_DEVICE	0x1
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_BASE		0x00000000
#endif /* CONFIG_CMD_NAND */

#ifdef CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_OFFSET		(CONFIG_U_BOOT_IMG_SIZE + CONFIG_SYS_NAND_U_BOOT_OFFS)
#define CONFIG_ENV_SIZE			SZ_128K
#define CONFIG_ENV_RANGE		(3 * CONFIG_SYS_NAND_BLOCK_SIZE)
#endif /* CONFIG_ENV_IS_IN_NAND */

#ifdef CONFIG_ENV_OFFSET_REDUND
#define CONFIG_SYS_ENV_PART_STR		xstr(CONFIG_SYS_ENV_PART_SIZE)	\
	"(env),"							\
	xstr(CONFIG_SYS_ENV_PART_SIZE)					\
	"(env2),"
#define CONFIG_SYS_USERFS_PART_STR	xstr(CONFIG_SYS_USERFS_PART_SIZE2) "(userfs)"
#else
#define CONFIG_SYS_ENV_PART_STR		xstr(CONFIG_SYS_ENV_PART_SIZE)	\
	"(env),"
#define CONFIG_SYS_USERFS_PART_STR	xstr(CONFIG_SYS_USERFS_PART_SIZE) "(userfs)"
#endif /* CONFIG_ENV_OFFSET_REDUND */

/*
 * MMC Driver
 */
#ifdef CONFIG_MXS_MMC
#ifdef CONFIG_CMD_MMC
#define CONFIG_CMD_FAT
#define CONFIG_FAT_WRITE
#define CONFIG_CMD_EXT2
#endif

/*
 * Environments on MMC
 */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_ENV_OVERWRITE
/* Associated with the MMC layout defined in mmcops.c */
#define CONFIG_ENV_OFFSET		SZ_1K
#define CONFIG_ENV_SIZE			(SZ_128K - CONFIG_ENV_OFFSET)
#define CONFIG_DYNAMIC_MMC_DEVNO
#endif /* CONFIG_ENV_IS_IN_MMC */
#endif /* CONFIG_MXS_MMC */

#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"1m@" xstr(CONFIG_SYS_NAND_U_BOOT_OFFS) "(u-boot),"		\
	CONFIG_SYS_ENV_PART_STR						\
	"6m(linux),32m(rootfs)," CONFIG_SYS_USERFS_PART_STR		\
	",512k@" xstr(CONFIG_SYS_NAND_DTB_OFFSET) "(dtb)"		\
	",512k@" xstr(CONFIG_SYS_NAND_BBT_OFFSET) "(bbt)ro"

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x1000 - /* Fix this */ \
					GENERATED_GBL_DATA_SIZE)

/* Defines for SPL */
#define CONFIG_SPL_START_S_PATH		"arch/arm/cpu/arm926ejs/mxs"
#define CONFIG_SPL_LDSCRIPT		"arch/arm/cpu/arm926ejs/mxs/u-boot-spl.lds"
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_GPIO_SUPPORT
#define CONFIG_SYS_SPL_VDDD_VAL		1500
#define CONFIG_SYS_SPL_BATT_BO_LEVEL	2400
#define CONFIG_SYS_SPL_VDDA_BO_VAL	100
#define CONFIG_SYS_SPL_VDDMEM_VAL	0	/* VDDMEM is not utilized on TX28 */

#endif /* __CONFIG_H */
