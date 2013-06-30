/*
 * Copyright (C) 2012 <LW@KARO-electronics.de>
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
#ifndef __TX6Q_H
#define __TX6Q_H

#include <asm/sizes.h>

/*
 * Ka-Ro TX6Q board - SoC configuration
 */
#define CONFIG_MX6
#define CONFIG_MX6Q
#define CONFIG_SYS_MX6_HCLK		24000000
#define CONFIG_SYS_MX6_CLK32		32768
#define CONFIG_SYS_HZ			1000		/* Ticks per second */
#define CONFIG_SHOW_ACTIVITY
#define CONFIG_ARCH_CPU_INIT
#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_BOARD_EARLY_INIT_F

#ifndef CONFIG_MFG
/* LCD Logo and Splash screen support */
#define CONFIG_LCD
#define CONFIG_SYS_CONSOLE_OVERWRITE_ROUTINE
#ifdef CONFIG_LCD
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_VIDEO_IPUV3
#define CONFIG_IPU_CLKRATE		266000000
#define CONFIG_LCD_LOGO
#define LCD_BPP				LCD_COLOR24
#define CONFIG_CMD_BMP
#define CONFIG_VIDEO_BMP_RLE8
#endif /* CONFIG_LCD */
#endif /* CONFIG_MFG */

/*
 * Memory configuration options
 */
#define CONFIG_NR_DRAM_BANKS		1		/* # of SDRAM banks */
#define PHYS_SDRAM_1			0x10000000	/* Base address of bank 1 */
#define PHYS_SDRAM_1_SIZE		SZ_1G
#define CONFIG_STACKSIZE		SZ_128K
#define CONFIG_SYS_MALLOC_LEN		SZ_8M
#define CONFIG_SYS_MEMTEST_START	PHYS_SDRAM_1	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + SZ_4M)
#define CONFIG_SYS_SDRAM_CLK		528

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_PROMPT		"TX6Q U-Boot > "
#define CONFIG_SYS_CBSIZE		2048		/* Console I/O buffer size */
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
				sizeof(CONFIG_SYS_PROMPT) + 16) /* Print buffer size */
#define CONFIG_SYS_MAXARGS		64		/* Max number of command args */
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
					/* Boot argument buffer size */
#define CONFIG_VERSION_VARIABLE		/* U-BOOT version */
#define CONFIG_AUTO_COMPLETE		/* Command auto complete */
#define CONFIG_CMDLINE_EDITING		/* Command history etc */

#define CONFIG_SYS_64BIT_VSPRINTF
#define CONFIG_SYS_NO_FLASH

/*
 * Flattened Device Tree (FDT) support
*/
#ifndef CONFIG_MFG
#define CONFIG_OF_LIBFDT
#ifdef CONFIG_OF_LIBFDT
#define CONFIG_FDT_FIXUP_PARTITIONS
#define CONFIG_OF_EMBED
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_DEFAULT_DEVICE_TREE	tx6q
#define CONFIG_ARCH_DEVICE_TREE		mx6q
#define CONFIG_SYS_FDT_ADDR		(PHYS_SDRAM_1 + SZ_16M)
#endif /* CONFIG_OF_LIBFDT */
#endif /* CONFIG_MFG */

/*
 * Boot Linux
 */
#define xstr(s)	str(s)
#define str(s)	#s
#define __pfx(x, s)			(x##s)
#define _pfx(x, s)			__pfx(x, s)

#define CONFIG_CMDLINE_TAG
#define CONFIG_INITRD_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_SERIAL_TAG
#ifndef CONFIG_MFG
#define CONFIG_BOOTDELAY		1
#else
#define CONFIG_BOOTDELAY		0
#endif
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_SYS_AUTOLOAD		"no"
#ifndef CONFIG_MFG
#define CONFIG_BOOTFILE			"uImage"
#define CONFIG_BOOTARGS			"console=ttymxc0,115200 ro debug panic=1"
#define CONFIG_BOOTCOMMAND		"run bootcmd_nand"
#else
#define CONFIG_BOOTCOMMAND		"env import " xstr(CONFIG_BOOTCMD_MFG_LOADADDR) ";run bootcmd_mfg"
#define CONFIG_BOOTCMD_MFG_LOADADDR	10500000
#define CONFIG_DELAY_ENVIRONMENT
#endif /* CONFIG_MFG */
#define CONFIG_LOADADDR			18000000
#define CONFIG_SYS_LOAD_ADDR		_pfx(0x, CONFIG_LOADADDR)
#define CONFIG_U_BOOT_IMG_SIZE		SZ_1M
#define CONFIG_IMX_WATCHDOG
#define CONFIG_WATCHDOG_TIMEOUT_MSECS	3000

/*
 * Extra Environments
 */
#ifndef CONFIG_MFG
#ifdef CONFIG_ENV_IS_NOWHERE
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"autoload=no\0"							\
	"bootdelay=-1\0"						\
	"fdtaddr=11000000\0"						\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"
#else
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"baseboard=stk5-v3\0"						\
	"bootargs_mmc=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/mmcblk0p3 rootwait\0"				\
	"bootargs_nand=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/mtdblock3 rootfstype=jffs2\0"			\
	"bootargs_nfs=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/nfs ip=dhcp nfsroot=${serverip}:${nfsroot},nolock\0"\
	"bootcmd_mmc=set autostart no;run bootargs_mmc;"		\
	"fatload mmc 0 ${loadaddr} uImage;run bootm_cmd\0"		\
	"bootcmd_nand=set autostart no;run bootargs_nand;"		\
	"nboot linux;run bootm_cmd\0"					\
	"bootcmd_net=set autostart no;run bootargs_nfs;dhcp;"		\
	"run bootm_cmd\0"						\
	"bootm_cmd=fdt boardsetup;bootm ${loadaddr} - ${fdtaddr}\0"	\
	"cpu_clk=800\0"							\
	"default_bootargs=set bootargs " CONFIG_BOOTARGS		\
	" video=${video_mode} ${append_bootargs}\0"			\
	"fdtaddr=11000000\0"						\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"				\
	"nfsroot=/tftpboot/rootfs\0"					\
	"otg_mode=device\0"						\
	"touchpanel=tsc2007\0"						\
	"video_mode=VGA-1:640x480MR-24@60\0"
#endif /*  CONFIG_ENV_IS_NOWHERE */
#endif /*  CONFIG_MFG */

#define MTD_NAME			"gpmi-nand"
#define MTDIDS_DEFAULT			"nand0=" MTD_NAME
#define CONFIG_SYS_NAND_ONFI_DETECTION

/*
 * U-Boot Commands
 */
#include <config_cmd_default.h>
#define CONFIG_CMD_CACHE
#define CONFIG_CMD_MMC
#define CONFIG_CMD_NAND
#define CONFIG_CMD_MTDPARTS
#define CONFIG_CMD_BOOTCE
#define CONFIG_CMD_TIME
#define CONFIG_CMD_I2C

/*
 * Serial Driver
 */
#define CONFIG_MXC_UART
#define CONFIG_MXC_UART_BASE		UART1_BASE
#define CONFIG_BAUDRATE			115200		/* Default baud rate */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200, }
#define CONFIG_SYS_CONSOLE_INFO_QUIET

/*
 * GPIO driver
 */
#define CONFIG_MXC_GPIO

/*
 * Ethernet Driver
 */
#define CONFIG_FEC_MXC
#ifdef CONFIG_FEC_MXC
/* This is required for the FEC driver to work with cache enabled */
#define CONFIG_SYS_ARM_CACHE_WRITETHROUGH

#define IMX_FEC_BASE			ENET_BASE_ADDR
#define CONFIG_FEC_MXC_PHYADDR		0
#define CONFIG_PHYLIB
#define CONFIG_PHY_SMSC
#define CONFIG_MII
#define CONFIG_FEC_XCV_TYPE		RMII
#define CONFIG_GET_FEC_MAC_ADDR_FROM_IIM
#define CONFIG_CMD_MII
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PING
/* Add for working with "strict" DHCP server */
#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_DNS
#endif

/*
 * I2C Configs
 */
#ifdef CONFIG_CMD_I2C
#define CONFIG_HARD_I2C			1
#define CONFIG_I2C_MXC			1
#define CONFIG_SYS_I2C_BASE		I2C1_BASE_ADDR
#define CONFIG_SYS_I2C_MX6_PORT1
#define CONFIG_SYS_I2C_SPEED		10000
#define CONFIG_SYS_I2C_SLAVE		0x3c
#define CONFIG_MX6_INTER_LDO_BYPASS	0
#endif

#ifndef CONFIG_ENV_IS_NOWHERE
/* define one of the following options:
#define CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_IS_IN_MMC
*/
#define CONFIG_ENV_IS_IN_NAND
#endif
#define CONFIG_ENV_OVERWRITE

/*
 * NAND flash driver
 */
#ifdef CONFIG_CMD_NAND
#define CONFIG_MTD_DEVICE
#if 0
#define CONFIG_MTD_DEBUG
#define CONFIG_MTD_DEBUG_VERBOSE	4
#endif
#define CONFIG_NAND_MXS
#define CONFIG_NAND_MXS_NO_BBM_SWAP
#define CONFIG_NAND_PAGE_SIZE		2048
#define CONFIG_NAND_OOB_SIZE		64
#define CONFIG_NAND_PAGES_PER_BLOCK	64
#define CONFIG_APBH_DMA
#define CONFIG_APBH_DMA_BURST
#define CONFIG_APBH_DMA_BURST8
#define CONFIG_SYS_NAND_U_BOOT_OFFS	0x20000
#define CONFIG_CMD_NAND_TRIMFFS
#define CONFIG_SYS_MXS_DMA_CHANNEL	4
#define CONFIG_SYS_MAX_FLASH_SECT	1024
#define CONFIG_SYS_MAX_FLASH_BANKS	1
#define CONFIG_SYS_NAND_MAX_CHIPS	1
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_USE_FLASH_BBT
#define CONFIG_SYS_NAND_BASE		0x00000000
#define CONFIG_CMD_ROMUPDATE
#else
#undef CONFIG_ENV_IS_IN_NAND
#endif /* CONFIG_CMD_NAND */

#define CONFIG_ENV_OFFSET		(CONFIG_U_BOOT_IMG_SIZE + CONFIG_SYS_NAND_U_BOOT_OFFS)
#define CONFIG_ENV_SIZE			SZ_128K
#define CONFIG_ENV_RANGE		0x60000
#ifdef CONFIG_ENV_OFFSET_REDUND
#define CONFIG_SYS_ENV_PART_STR		xstr(CONFIG_ENV_RANGE)		\
	"(env),"							\
	xstr(CONFIG_ENV_RANGE)						\
	"(env2),"
#define CONFIG_SYS_USERFS_PART_STR	"91520k(userfs)"
#else
#define CONFIG_SYS_ENV_PART_STR		xstr(CONFIG_ENV_RANGE)		\
	"(env),"
#define CONFIG_SYS_USERFS_PART_STR	"91904k(userfs)"
#endif /* CONFIG_ENV_OFFSET_REDUND */

/*
 * MMC Driver
 */
#ifdef CONFIG_CMD_MMC
#define CONFIG_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR	0
#define CONFIG_SYS_FSL_ESDHC_NUM	2

#define CONFIG_DOS_PARTITION
#define CONFIG_CMD_FAT
#define CONFIG_CMD_EXT2

/*
 * Environments on MMC
 */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		0
#undef CONFIG_ENV_OFFSET
#undef CONFIG_ENV_SIZE
/* Associated with the MMC layout defined in mmcops.c */
#define CONFIG_ENV_OFFSET		SZ_1K
#define CONFIG_ENV_SIZE			(SZ_128K - CONFIG_ENV_OFFSET)
#define CONFIG_DYNAMIC_MMC_DEVNO
#endif /* CONFIG_ENV_IS_IN_MMC */
#else
#undef CONFIG_ENV_IS_IN_MMC
#endif /* CONFIG_CMD_MMC */

#ifdef CONFIG_ENV_IS_NOWHERE
#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_SIZE			SZ_4K
#endif

#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"1m@" xstr(CONFIG_SYS_NAND_U_BOOT_OFFS) "(u-boot),"		\
	CONFIG_SYS_ENV_PART_STR						\
	"4m(linux),32m(rootfs),256k(dtb),"				\
	CONFIG_SYS_USERFS_PART_STR ",512k@0x7f80000(bbt)ro"

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x1000 - /* Fix this */ \
					GENERATED_GBL_DATA_SIZE)

#ifdef CONFIG_CMD_IIM
#define CONFIG_IMX_IIM
#endif

#endif /* __CONFIG_H */
