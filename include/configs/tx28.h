/*
 * Copyright (C) 2012 <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:      GPL-2.0
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_MX28			/* must be defined before including regs-base.h */

#include <asm/sizes.h>
#include <asm/arch/regs-base.h>

/*
 * Ka-Ro TX28 board - SoC configuration
 */
#define CONFIG_MXS_GPIO					/* GPIO control */
#define CONFIG_SYS_HZ			1000		/* Ticks per second */
#define PHYS_SDRAM_1_SIZE		CONFIG_SDRAM_SIZE
#ifdef CONFIG_TX28_S
#define TX28_MOD_SUFFIX			"1"
#else
#define CONFIG_SYS_SPL_FIXED_BATT_SUPPLY
#define TX28_MOD_SUFFIX			"0"
#endif

#ifndef CONFIG_SPL_BUILD
#define CONFIG_SKIP_LOWLEVEL_INIT
#define CONFIG_SHOW_ACTIVITY
#define CONFIG_ARCH_CPU_INIT
#define CONFIG_DISPLAY_CPUINFO
#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_BOARD_EARLY_INIT_F

/* LCD Logo and Splash screen support */
#define CONFIG_LCD
#ifdef CONFIG_LCD
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_VIDEO_MXS
#define CONFIG_LCD_LOGO
#define LCD_BPP				LCD_COLOR24
#define CONFIG_CMD_BMP
#define CONFIG_VIDEO_BMP_RLE8
#endif /* CONFIG_LCD */
#endif /* CONFIG_SPL_BUILD */

/*
 * Memory configuration options
 */
#define CONFIG_NR_DRAM_BANKS		1		/* 1 bank of SDRAM */
#define PHYS_SDRAM_1			0x40000000	/* SDRAM Bank #1 */
#define CONFIG_STACKSIZE		SZ_64K
#define CONFIG_SYS_MALLOC_LEN		SZ_4M
#define CONFIG_SYS_MEMTEST_START	PHYS_SDRAM_1	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + SZ_4M)

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_PROMPT		"TX28 U-Boot > "
#define CONFIG_SYS_CBSIZE		2048		/* Console I/O buffer size */
#define CONFIG_SYS_PBSIZE \
	(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
							/* Print buffer size */
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
#define CONFIG_OF_LIBFDT
#ifdef CONFIG_OF_LIBFDT
#define CONFIG_FDT_FIXUP_PARTITIONS
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_DEFAULT_DEVICE_TREE	tx28
#define CONFIG_ARCH_DEVICE_TREE		mx28
#define CONFIG_SYS_FDT_ADDR		(PHYS_SDRAM_1 + SZ_16M)
#endif

/*
 * Boot Linux
 */
#define xstr(s)	str(s)
#define str(s)	#s
#define __pfx(x, s)			(x##s)
#define _pfx(x, s)			__pfx(x, s)

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_BOOTDELAY		3
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_SYS_AUTOLOAD		"no"
#define CONFIG_BOOTFILE			"uImage"
#define CONFIG_BOOTARGS			"console=ttyAMA0,115200 ro debug panic=1"
#define CONFIG_BOOTCOMMAND		"run bootcmd_nand"
#define CONFIG_LOADADDR			43000000
#define CONFIG_SYS_LOAD_ADDR		_pfx(0x, CONFIG_LOADADDR)
#define CONFIG_U_BOOT_IMG_SIZE		SZ_1M

/*
 * Extra Environments
 */
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
	" root=/dev/nfs ip=dhcp nfsroot=${nfs_server}:${nfsroot},nolock\0" \
	"bootcmd_mmc=set autostart no;run bootargs_mmc;"		\
	"fatload mmc 0 ${loadaddr} uImage;run bootm_cmd\0"		\
	"bootcmd_nand=set autostart no;run bootargs_nand;"		\
	"nboot linux;run bootm_cmd\0"					\
	"bootcmd_net=set autostart no;run bootargs_nfs;dhcp;"		\
	"run bootm_cmd\0"						\
	"bootm_cmd=bootm ${loadaddr} - ${fdtaddr}\0"			\
	"default_bootargs=set bootargs " CONFIG_BOOTARGS		\
	" ${append_bootargs}\0"		\
	"fdtaddr=41000000\0"						\
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
 * U-Boot Commands
 */
#include <config_cmd_default.h>
#define CONFIG_CMD_CACHE
#define CONFIG_CMD_MMC
#define CONFIG_CMD_NAND
#define CONFIG_CMD_MTDPARTS
#define CONFIG_CMD_BOOTCE
#define CONFIG_CMD_TIME
#define CONFIG_CMD_MEMTEST

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
#define CONFIG_FEC_MXC
#ifdef CONFIG_FEC_MXC
/* This is required for the FEC driver to work with cache enabled */
#define CONFIG_SYS_ARM_CACHE_WRITETHROUGH
#define CONFIG_SYS_CACHELINE_SIZE	32

#ifndef CONFIG_TX28_S
#define CONFIG_FEC_MXC_MULTI
#else
#define IMX_FEC_BASE			MXS_ENET0_BASE
#define CONFIG_FEC_MXC_PHYADDR		0x00
#endif

#define CONFIG_PHY_SMSC
#define CONFIG_PHYLIB
#define CONFIG_MII
#define CONFIG_FEC_XCV_TYPE		RMII
#define CONFIG_GET_FEC_MAC_ADDR_FROM_IIM
#define CONFIG_NET_MULTI
#define CONFIG_CMD_MII
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PING
/* Add for working with "strict" DHCP server */
#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_DNS
#define CONFIG_BOOTP_RANDOM_ID
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
#define CONFIG_NAND_MXS
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
#define CONFIG_SYS_USERFS_PART_STR	"107904k(userfs)"
#else
#define CONFIG_SYS_ENV_PART_STR		xstr(CONFIG_ENV_RANGE)		\
	"(env),"
#define CONFIG_SYS_USERFS_PART_STR	"108288k(userfs)"
#endif /* CONFIG_ENV_OFFSET_REDUND */

/*
 * MMC Driver
 */
#ifdef CONFIG_CMD_MMC
#define CONFIG_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_MXS_MMC
#define CONFIG_BOUNCE_BUFFER

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
	"4m(linux),16m(rootfs),"					\
	CONFIG_SYS_USERFS_PART_STR ",256k(dtb),512k@0x7f80000(bbt)ro"

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x1000 - /* Fix this */ \
					GENERATED_GBL_DATA_SIZE)

/* Defines for SPL */
#define CONFIG_SPL
#define CONFIG_SPL_NO_CPU_SUPPORT_CODE
#define CONFIG_SPL_START_S_PATH	"arch/arm/cpu/arm926ejs/mxs"
#define CONFIG_SPL_LDSCRIPT	"arch/arm/cpu/arm926ejs/mxs/u-boot-spl.lds"
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_GPIO_SUPPORT
#define CONFIG_SYS_SPL_VDDD_VAL		1500
#define CONFIG_SYS_SPL_BATT_BO_LEVEL	2800
#define CONFIG_SYS_SPL_VDDMEM_VAL	0	/* VDDMEM is not utilized on TX28 */

#endif /* __CONFIGS_TX28_H */
