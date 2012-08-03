/*
 * tx48.h
 *
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * based on: am335x_evm
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#ifndef __TX48_H
#define __TX48_H

#include <config.h>
#include <asm/sizes.h>

/*
 * Ka-Ro TX48 board - SoC configuration
 */
#define CONFIG_AM33XX
#define CONFIG_AM33XX_GPIO
#define CONFIG_SYS_HZ			1000		/* Ticks per second */
#define CONFIG_SHOW_ACTIVITY
#define CONFIG_DISPLAY_CPUINFO
#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_VIDEO_DA8XX
#define DAVINCI_LCD_CNTL_BASE		0x4830e000

/* LCD Logo and Splash screen support */
#define CONFIG_LCD
#ifdef CONFIG_LCD
#define CONFIG_LCD_LOGO
#define LCD_BPP				LCD_COLOR24
#define CONFIG_CMD_BMP
#define CONFIG_VIDEO_BMP_RLE8
#endif /* CONFIG_LCD */

/* Clock Defines */
#define V_OSCK				24000000  /* Clock output from T2 */
#define V_SCLK				V_OSCK

/*
 * Memory configuration options
 */
#define CONFIG_SYS_SDRAM_DDR3
#define CONFIG_NR_DRAM_BANKS		1		/*  1 bank of SDRAM */
#define PHYS_SDRAM_1			0x80000000	/* SDRAM Bank #1 */
#define PHYS_SDRAM_1_SIZE		SZ_256M
#define CONFIG_MAX_RAM_BANK_SIZE	SZ_1G

#define CONFIG_STACKSIZE		SZ_64K
#define CONFIG_SYS_MALLOC_LEN		SZ_4M

#define CONFIG_SYS_MEMTEST_START	(PHYS_SDRAM_1 + (64 * 1024 * 1024))
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + \
					(8 * 1024 * 1024))


/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_PROMPT	"TX48 U-Boot > "
#define CONFIG_SYS_CBSIZE	2048		/* Console I/O buffer size */
#define CONFIG_SYS_PBSIZE \
	(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
						/* Print buffer size */
#define CONFIG_SYS_MAXARGS	64		/* Max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE
						/* Boot argument buffer size */
#define CONFIG_VERSION_VARIABLE			/* U-BOOT version */
#define CONFIG_AUTO_COMPLETE			/* Command auto complete */
#define CONFIG_CMDLINE_EDITING			/* Command history etc */

#define CONFIG_SYS_64BIT_VSPRINTF
#define CONFIG_SYS_NO_FLASH

/*
 * Flattened Device Tree (FDT) support
*/
#ifdef CONFIG_OF_LIBFDT /* set via cmdline parameter thru boards.cfg */
#define CONFIG_FDT_FIXUP_PARTITIONS
#define CONFIG_OF_CONTROL
#define CONFIG_OF_EMBED
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_DEFAULT_DEVICE_TREE	tx48
#define CONFIG_ARCH_DEVICE_TREE		am33xx
#define CONFIG_MACH_TYPE		(-1)
#else
#ifndef MACH_TYPE_TIAM335EVM
#define MACH_TYPE_TIAM335EVM		 3589	 /* Until the next sync */
#endif
#define CONFIG_MACH_TYPE	 MACH_TYPE_TIAM335EVM
#endif

/*
 * Boot Linux
 */
#define xstr(s)	str(s)
#define str(s)	#s

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_BOOTDELAY	3
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_SYS_AUTOLOAD	"no"
#define CONFIG_BOOTFILE		"uImage"
#define CONFIG_BOOTARGS		"console=ttymO0,115200 ro debug panic=1"
#define CONFIG_BOOTCOMMAND	"run bootcmd_nand"
#define CONFIG_LOADADDR		0x81000000
#define CONFIG_SYS_LOAD_ADDR	CONFIG_LOADADDR
#define CONFIG_U_BOOT_IMG_SIZE	SZ_1M
#define CONFIG_HW_WATCHDOG

/*
 * Extra Environments
 */
#ifdef CONFIG_OF_LIBFDT
#define CONFIG_FDT_FIXUP_PARTITIONS

#define TX48_BOOTM_CMD							\
	"bootm_cmd=fdt addr ${fdtcontroladdr};"				\
	"fdt board;"							\
	"bootm ${loadaddr} - ${fdtaddr}\0"
#else
#define TX48_BOOTM_CMD							\
	"bootm_cmd=bootm\0"
#endif

#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"baseboard=stk5-v3\0"						\
	"bootargs_mmc=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/mmcblk0p2 rootwait\0"				\
	"bootargs_nand=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/mtdblock4 rootfstype=jffs2\0"		\
	"nfsroot=/tftpboot/rootfs\0"					\
	"bootargs_nfs=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/nfs ip=dhcp nfsroot=${serverip}:${nfsroot},nolock\0"\
	"bootcmd_mmc=set autostart no;run bootargs_mmc;"		\
	"fatload mmc 0 ${loadaddr} uImage;run bootm_cmd\0"		\
	"bootcmd_nand=set autostart no;run bootargs_nand;"		\
	"nboot linux;run bootm_cmd\0"					\
	"bootcmd_net=set autostart no;run bootargs_nfs;dhcp;"		\
	"run bootm_cmd\0"						\
	TX48_BOOTM_CMD							\
	"default_bootargs=set bootargs " CONFIG_BOOTARGS		\
	" ${mtdparts} video=${video_mode} ${append_bootargs}\0"		\
	"fdtcontroladdr=80004000\0"					\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"				\
	"touchpanel=tsc2007\0"						\
	"video_mode=640x480MR-24@60\0"

#define MTD_NAME			"omap2-nand.0"
#define MTDIDS_DEFAULT			"nand0=" MTD_NAME

/*
 * U-Boot Commands
 */
#include <config_cmd_default.h>
#define CONFIG_CMD_CACHE
#define CONFIG_CMD_MMC
#define CONFIG_CMD_NAND
#define CONFIG_CMD_MTDPARTS
#define CONFIG_CMD_BOOTCE

/*
 * Serial Driver
 */
#define CONFIG_SYS_NS16550
#define CONFIG_SYS_NS16550_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE	(-4)
#define CONFIG_SYS_NS16550_CLK		48000000
#define CONFIG_SYS_NS16550_COM1		0x44e09000	/* UART0 */

/*
 * Ethernet Driver
 */
#ifdef CONFIG_CMD_NET
#define CONFIG_DRIVER_TI_CPSW
#define CONFIG_MII
#define CONFIG_CMD_MII
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PING
#define CONFIG_BOOTP_DNS
#define CONFIG_BOOTP_DNS2
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_SUBNETMASK

#define CONFIG_NET_RETRY_COUNT	       10
#define CONFIG_NET_MULTI
#define CONFIG_PHY_GIGE
#endif

/*
 * NAND flash driver
 */
#ifdef CONFIG_CMD_NAND
#define CONFIG_MTD_DEVICE
#define CONFIG_ENV_IS_IN_NAND
#define CONFIG_NAND_AM33XX
#define GPMC_NAND_ECC_LP_x16_LAYOUT
#define CONFIG_SYS_NAND_U_BOOT_OFFS	0x20000
#define CONFIG_SYS_NAND_PAGE_SIZE	2048
#define CONFIG_SYS_NAND_OOBSIZE		64
#define CONFIG_SYS_NAND_ECCSIZE		512
#define CONFIG_SYS_NAND_ECCBYTES	14
#define CONFIG_SYS_NAND_ECCSTEPS	4

#define CONFIG_SYS_MAX_FLASH_SECT	1024
#define CONFIG_SYS_MAX_FLASH_BANKS	1
#define CONFIG_SYS_NAND_MAX_CHIPS	1
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_USE_FLASH_BBT
#ifdef CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_OFFSET		(CONFIG_U_BOOT_IMG_SIZE + CONFIG_SYS_NAND_U_BOOT_OFFS)
#define CONFIG_ENV_SIZE			0x20000 /* 128 KiB */
#if 0
#define CONFIG_ENV_OFFSET_REDUND	0x20000
#define CONFIG_ENV_SIZE_REDUND		CONFIG_ENV_SIZE
#endif
#endif
#define CONFIG_SYS_NAND_BASE		0x08000000 /* must be defined but value is irrelevant */
#define NAND_BASE			CONFIG_SYS_NAND_BASE
#endif /* CONFIG_CMD_NAND */

/*
 * MMC Driver
 */
#ifdef CONFIG_CMD_MMC
#ifndef CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_IS_IN_MMC
#endif
#define CONFIG_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_OMAP_HSMMC
#define CONFIG_OMAP_MMC_DEV_1

#define CONFIG_BOOT_PARTITION_ACCESS
#define CONFIG_DOS_PARTITION
#define CONFIG_CMD_FAT
#define CONFIG_CMD_EXT2

/*
 * Environments on MMC
 */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV	0
#define CONFIG_ENV_OVERWRITE
/* Associated with the MMC layout defined in mmcops.c */
#define CONFIG_ENV_OFFSET		0x400 /* 1 KB */
#define CONFIG_ENV_SIZE			(0x20000 - 0x400) /* 127 KB */
#define CONFIG_DYNAMIC_MMC_DEVNO
#endif /* CONFIG_ENV_IS_IN_MMC */
#endif /* CONFIG_CMD_MMC */

#ifdef CONFIG_ENV_OFFSET_REDUND
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"128k(u-boot-spl),"						\
	"1m(u-boot),"							\
	xstr(CONFIG_ENV_SIZE)						\
	"(env),"							\
	xstr(CONFIG_ENV_SIZE)						\
	"(env2),4m(linux),16m(rootfs),-(userfs)"
#else
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"128k(u-boot-spl),"						\
	"1m(u-boot),"							\
	xstr(CONFIG_ENV_SIZE)						\
	"(env),4m(linux),16m(rootfs),-(userfs)"
#endif

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define SRAM0_SIZE			SZ_64K
#define CONFIG_SYS_INIT_SP_ADDR		0x4030B7FC
#define CONFIG_SPL_STACK		CONFIG_SYS_INIT_SP_ADDR

 /* Platform/Board specific defs */
#define CONFIG_SYS_TIMERBASE		0x48040000	/* Use Timer2 */
#define CONFIG_SYS_PTV			2	/* Divisor: 2^(PTV+1) => 8 */

#define CONFIG_BAUDRATE		115200
#define CONFIG_SYS_BAUDRATE_TABLE	{ 110, 300, 600, 1200, 2400, \
		4800, 9600, 14400, 19200, 28800, 38400, 56000, 57600, 115200 }

/*
 * select serial console configuration
 */
#define CONFIG_CONS_INDEX		1
#define CONFIG_SYS_CONSOLE_INFO_QUIET

/* Defines for SPL */
#define CONFIG_SPL
#define CONFIG_SPL_BOARD_INIT
#define CONFIG_SPL_TEXT_BASE		0x402F0400
#define CONFIG_SPL_MAX_SIZE		(46 * 1024)
#define CONFIG_SPL_GPIO_SUPPORT
#ifdef CONFIG_NAND_AM33XX
#define CONFIG_SPL_NAND_SUPPORT
#define CONFIG_SPL_NAND_SIMPLE
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_PAGE_COUNT	(CONFIG_SYS_NAND_BLOCK_SIZE /	\
					CONFIG_SYS_NAND_PAGE_SIZE)
#define CONFIG_SYS_NAND_BLOCK_SIZE	SZ_128K
#define CONFIG_SYS_NAND_BAD_BLOCK_POS	NAND_LARGE_BADBLOCK_POS
#define CONFIG_SYS_NAND_ECCPOS		{ 2, 3, 4, 5, 6, 7, 8, 9, \
					 10, 11, 12, 13, 14, 15, 16, 17, \
					 18, 19, 20, 21, 22, 23, 24, 25, \
					 26, 27, 28, 29, 30, 31, 32, 33, \
					 34, 35, 36, 37, 38, 39, 40, 41, \
					 42, 43, 44, 45, 46, 47, 48, 49, \
					 50, 51, 52, 53, 54, 55, 56, 57, }
#endif

#define CONFIG_SPL_BSS_START_ADDR	0x80000000
#define CONFIG_SPL_BSS_MAX_SIZE		0x80000		/* 512 KB */

#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x300 /* address 0x60000 */
#define CONFIG_SYS_U_BOOT_MAX_SIZE_SECTORS	0x200 /* 256 KB */
#define CONFIG_SYS_MMC_SD_FAT_BOOT_PARTITION	1
#define CONFIG_SPL_FAT_LOAD_PAYLOAD_NAME	"u-boot-tx48.img"
#define CONFIG_SPL_MMC_SUPPORT
#define CONFIG_SPL_FAT_SUPPORT
#define CONFIG_SPL_I2C_SUPPORT

#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBDISK_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_YMODEM_SUPPORT
#define CONFIG_SPL_LDSCRIPT		"$(CPUDIR)/omap-common/u-boot-spl.lds"

/*
 * 1MB into the SDRAM to allow for SPL's bss at the beginning of SDRAM
 * 64 bytes before this address should be set aside for u-boot.img's
 * header. That is 0x800FFFC0--0x80100000 should not be used for any
 * other needs.
 */
#define CONFIG_SYS_TEXT_BASE		0x80800000
#define CONFIG_SYS_SPL_MALLOC_START	0x80208000
#define CONFIG_SYS_SPL_MALLOC_SIZE	0x100000

/* Since SPL did pll and ddr initialization for us,
 * we don't need to do it twice.
 */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_SKIP_LOWLEVEL_INIT
#endif

/* Unsupported features */
#undef CONFIG_USE_IRQ

#endif	/* __TX48_H */
