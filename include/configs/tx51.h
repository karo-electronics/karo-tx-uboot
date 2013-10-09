/*
 * Copyright (C) 2012 <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:      GPL-2.0
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_MX51			/* must be set before including imx-regs.h */

#include <asm/sizes.h>
#include <asm/arch/imx-regs.h>

/*
 * Ka-Ro TX51 board - SoC configuration
 */
#define CONFIG_SYS_MX5_IOMUX_V3
#define CONFIG_MXC_GPIO			/* GPIO control */
#define CONFIG_SYS_MX5_HCLK		24000000
#define CONFIG_SYS_DDR_CLKSEL		0
#define CONFIG_SYS_HZ			1000	/* Ticks per second */
#define CONFIG_SHOW_ACTIVITY
#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_BOARD_EARLY_INIT_F

#if CONFIG_SYS_CPU_CLK == 600
#define TX51_MOD_PREFIX			"6"
#elif CONFIG_SYS_CPU_CLK == 800
#define TX51_MOD_PREFIX			"8"
#define CONFIG_MX51_PLL_ERRATA
#else
#error Invalid CPU clock
#endif

/* LCD Logo and Splash screen support */
#define CONFIG_LCD
#ifdef CONFIG_LCD
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_VIDEO_IPUV3
#define CONFIG_IPUV3_CLK		200000000
#define CONFIG_LCD_LOGO
#define LCD_BPP				LCD_COLOR24
#define CONFIG_CMD_BMP
#define CONFIG_VIDEO_BMP_RLE8
#endif /* CONFIG_LCD */

/*
 * Memory configurations
 */
#define PHYS_SDRAM_1			0x90000000	/* Base address of bank 1 */
#define PHYS_SDRAM_1_SIZE		SZ_128M
#if CONFIG_NR_DRAM_BANKS > 1
#define PHYS_SDRAM_2			0x98000000	/* Base address of bank 2 */
#define PHYS_SDRAM_2_SIZE		SZ_128M
#else
#define TX51_MOD_SUFFIX			"0"
#endif
#define CONFIG_STACKSIZE		SZ_128K
#define CONFIG_SYS_MALLOC_LEN		SZ_8M
#define CONFIG_SYS_MEMTEST_START	PHYS_SDRAM_1	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END		(PHYS_SDRAM_1 + SZ_4M)	/* 4 MB RAM test */
#if CONFIG_SYS_SDRAM_CLK == 200
#define CONFIG_SYS_CLKTL_CBCDR		0x59e35180
#define TX51_MOD_SUFFIX			"1"
#elif CONFIG_SYS_SDRAM_CLK == 166
#define CONFIG_SYS_CLKTL_CBCDR		0x01e35180
#ifndef TX51_MOD_SUFFIX
#define TX51_MOD_SUFFIX			"2"
#endif
#else
#error Invalid SDRAM clock
#endif

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_PROMPT		"TX51 U-Boot > "
#define CONFIG_SYS_CBSIZE		2048	/* Console I/O buffer size */
#define CONFIG_SYS_PBSIZE \
	(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
						/* Print buffer size */
#define CONFIG_SYS_MAXARGS		64	/* Max number of command args */
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
						/* Boot argument buffer size */
#define CONFIG_VERSION_VARIABLE			/* U-BOOT version */
#define CONFIG_AUTO_COMPLETE			/* Command auto complete */
#define CONFIG_CMDLINE_EDITING			/* Command history etc */

#define CONFIG_SYS_64BIT_VSPRINTF
#define CONFIG_SYS_NO_FLASH

/*
 * Flattened Device Tree (FDT) support
*/
#define CONFIG_OF_LIBFDT
#define CONFIG_OF_EMBED
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_DEFAULT_DEVICE_TREE	tx51
#define CONFIG_ARCH_DEVICE_TREE		mx51
#define CONFIG_SYS_FDT_ADDR		(PHYS_SDRAM_1 + SZ_16M)

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
#define CONFIG_BOOTARGS			"console=ttymxc0,115200 ro debug panic=1"
#define CONFIG_BOOTCOMMAND		"run bootcmd_nand"
#define CONFIG_LOADADDR			94000000
#define CONFIG_SYS_LOAD_ADDR		_pfx(0x, CONFIG_LOADADDR)
#define CONFIG_U_BOOT_IMG_SIZE		SZ_1M
#define CONFIG_HW_WATCHDOG

/*
 * Extra Environments
 */
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"baseboard=stk5-v3\0"						\
	"bootargs_mmc=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/mmcblk0p3 rootwait\0"				\
	"bootargs_nand=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/mtdblock3 rootfstype=jffs2\0"			\
	"nfsroot=/tftpboot/rootfs\0"					\
	"bootargs_nfs=run default_bootargs;set bootargs ${bootargs}"	\
	" root=/dev/nfs ip=dhcp nfsroot=${nfs_server}:${nfsroot},nolock\0"\
	"bootcmd_mmc=set autostart no;run bootargs_mmc;"		\
	"mmc read ${loadaddr} 100 3000;run bootm_cmd\0"			\
	"bootcmd_nand=set autostart no;run bootargs_nand;"		\
	"nboot linux;run bootm_cmd\0"					\
	"bootcmd_net=set autostart no;run bootargs_nfs;dhcp;"		\
	"run bootm_cmd\0"						\
	"bootm_cmd=bootm ${loadaddr} - ${fdtaddr}\0"			\
	"default_bootargs=set bootargs " CONFIG_BOOTARGS		\
	" video=${video_mode} ${append_bootargs}\0"			\
	"cpu_clk=" xstr(CONFIG_SYS_CPU_CLK) "\0"			\
	"fdtaddr=91000000\0"						\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"				\
	"otg_mode=device\0"						\
	"touchpanel=tsc2007\0"						\
	"video_mode=VGA-1:640x480MR-24@60\0"

#define MTD_NAME			"mxc_nand"
#define MTDIDS_DEFAULT			"nand0=" MTD_NAME
#define CONFIG_FDT_FIXUP_PARTITIONS

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
#define CONFIG_MXC_UART
#define CONFIG_MXC_UART_BASE		UART1_BASE
#define CONFIG_MXC_GPIO
#define CONFIG_BAUDRATE			115200		/* Default baud rate */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200, }
#define CONFIG_SYS_CONSOLE_INFO_QUIET

/*
 * Ethernet Driver
 */
#define CONFIG_FEC_MXC
#ifdef CONFIG_FEC_MXC
#define IMX_FEC_BASE			FEC_BASE_ADDR
#define CONFIG_FEC_MXC_PHYADDR		0x1f
#define CONFIG_PHYLIB
#define CONFIG_PHY_SMSC
#define CONFIG_MII
#define CONFIG_FEC_XCV_TYPE		MII100
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
 * NAND flash driver
 */
#ifdef CONFIG_CMD_NAND
#define CONFIG_MTD_DEVICE
#define CONFIG_ENV_IS_IN_NAND
#define CONFIG_NAND_MXC
#define CONFIG_MXC_NAND_REGS_BASE	NFC_BASE_ADDR_AXI
#define CONFIG_MXC_NAND_IP_REGS_BASE	NFC_BASE_ADDR
#define CONFIG_MXC_NAND_HWECC
#define CONFIG_CMD_NAND_TRIMFFS
#define CONFIG_SYS_MAX_FLASH_SECT	1024
#define CONFIG_SYS_MAX_FLASH_BANKS	1
#define CONFIG_SYS_NAND_MAX_CHIPS	1
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_USE_FLASH_BBT
#ifdef CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_OFFSET		CONFIG_U_BOOT_IMG_SIZE
#define CONFIG_ENV_SIZE			0x20000 /* 128 KiB */
#define CONFIG_ENV_RANGE		0x60000
#endif
#ifndef CONFIG_SYS_NO_FLASH
#define CONFIG_CMD_FLASH
#define CONFIG_SYS_NAND_BASE		0xa0000000
#define CONFIG_FIT
#else
#define CONFIG_SYS_NAND_BASE		0x00000000
#define CONFIG_CMD_ROMUPDATE
#endif
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
#define CONFIG_FSL_ESDHC
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
#define CONFIG_ENV_OVERWRITE
/* Associated with the MMC layout defined in mmcops.c */
#define CONFIG_ENV_OFFSET		SZ_1K
#define CONFIG_ENV_SIZE			(SZ_128K - CONFIG_ENV_OFFSET)
#define CONFIG_DYNAMIC_MMC_DEVNO
#endif /* CONFIG_ENV_IS_IN_MMC */
#endif /* CONFIG_CMD_MMC */

#ifdef CONFIG_ENV_OFFSET_REDUND
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"1m(u-boot),"							\
	xstr(CONFIG_ENV_RANGE)						\
	"(env),"							\
	xstr(CONFIG_ENV_RANGE)						\
	"(env2),4m(linux),16m(rootfs),256k(dtb),?(userfs),512k@0x7f80000(bbt)ro"
#else
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"1m(u-boot),"							\
	xstr(CONFIG_ENV_RANGE)						\
	"(env),4m(linux),16m(rootfs),256k(dtb),?(userfs),512k@0x7f80000(bbt)ro"
#endif

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x1000 - /* Fix this */ \
					GENERATED_GBL_DATA_SIZE)

#ifdef CONFIG_CMD_IIM
#define CONFIG_FSL_IIM
#endif

#endif /* __CONFIG_H */
