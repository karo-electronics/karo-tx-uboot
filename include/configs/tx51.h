/*
 * Copyright (C) 2012-2014 <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:      GPL-2.0
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/kconfig.h>
#include <linux/sizes.h>
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
#define CONFIG_SYS_GENERIC_BOARD

#if CONFIG_SYS_CPU_CLK == 600
#define TX51_MOD_PREFIX			"6"
#elif CONFIG_SYS_CPU_CLK == 800
#define TX51_MOD_PREFIX			"8"
#define CONFIG_MX51_PLL_ERRATA
#else
#error Invalid CPU clock
#endif

/* LCD Logo and Splash screen support */
#ifdef CONFIG_LCD
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_VIDEO_IPUV3
#define CONFIG_IPUV3_CLK		133000000
#define CONFIG_LCD_LOGO
#define LCD_BPP				LCD_COLOR32
#define CONFIG_CMD_BMP
#define CONFIG_VIDEO_BMP_RLE8
#endif /* CONFIG_LCD */

/*
 * Memory configuration options
 */
#define PHYS_SDRAM_1			0x90000000	/* Base address of bank 1 */
#define PHYS_SDRAM_1_SIZE		SZ_128M
#if CONFIG_NR_DRAM_BANKS > 1
#define PHYS_SDRAM_2			0x98000000	/* Base address of bank 2 */
#define PHYS_SDRAM_2_SIZE		SZ_128M
#endif
#define CONFIG_STACKSIZE		SZ_128K
#define CONFIG_SYS_MALLOC_LEN		SZ_8M
#define CONFIG_SYS_MEMTEST_START	PHYS_SDRAM_1	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END		(PHYS_SDRAM_1 + SZ_4M)	/* 4 MB RAM test */
#define CONFIG_SYS_SDRAM_CLK		166
#define CONFIG_SYS_CLKTL_CBCDR		0x01e35100

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_CBSIZE		2048	/* Console I/O buffer size */
#define CONFIG_SYS_PBSIZE \
	(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
						/* Print buffer size */
#define CONFIG_SYS_MAXARGS		256	/* Max number of command args */
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
						/* Boot argument buffer size */
#define CONFIG_VERSION_VARIABLE			/* U-BOOT version */
#define CONFIG_AUTO_COMPLETE			/* Command auto complete */
#define CONFIG_CMDLINE_EDITING			/* Command history etc */

#define CONFIG_SYS_64BIT_VSPRINTF

/*
 * Flattened Device Tree (FDT) support
*/

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
#define CONFIG_BOOTARGS			"init=/linuxrc console=ttymxc0,115200 ro debug panic=1"
#define CONFIG_BOOTCOMMAND		"run bootcmd_${boot_mode} bootm_cmd"
#define CONFIG_LOADADDR			94000000
#define CONFIG_FDTADDR			91000000
#define CONFIG_SYS_LOAD_ADDR		_pfx(0x, CONFIG_LOADADDR)
#define CONFIG_SYS_FDT_ADDR		_pfx(0x, CONFIG_FDTADDR)
#define CONFIG_U_BOOT_IMG_SIZE		SZ_1M

/*
 * Extra Environment Settings
 */
#ifdef CONFIG_TX51_UBOOT_NOENV
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"autoload=no\0"							\
	"baseboard=stk5-v3\0"						\
	"bootdelay=-1\0"						\
	"fdtaddr=" xstr(CONFIG_FDTADDR) "\0"				\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"
#else
#define CONFIG_SYS_CPU_CLK_STR		xstr(CONFIG_SYS_MPU_CLK)

#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"baseboard=stk5-v3\0"						\
	"bootargs_jffs2=run default_bootargs"				\
	";setenv bootargs ${bootargs}"					\
	" root=/dev/mtdblock3 rootfstype=jffs2\0"			\
	"bootargs_mmc=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=/dev/mmcblk0p2 rootwait\0"				\
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
	"bootcmd_net=setenv autoload y"					\
	";setenv autostart n;run bootargs_nfs"				\
	";dhcp\0"							\
	"bootm_cmd=bootm ${loadaddr} - ${fdtaddr}\0"			\
	"boot_mode=nand\0"						\
	"cpu_clk=" CONFIG_SYS_CPU_CLK_STR "\0"				\
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
#endif /*  CONFIG_TX51_UBOOT_NOENV */

#define MTD_NAME			"mxc_nand"
#define MTDIDS_DEFAULT			"nand0=" MTD_NAME

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
#ifdef CONFIG_FEC_MXC
#define IMX_FEC_BASE			FEC_BASE_ADDR
#define CONFIG_FEC_XCV_TYPE		MII100
#endif

/*
 * NAND flash driver
 */
#ifdef CONFIG_CMD_NAND
#define CONFIG_MXC_NAND_REGS_BASE	NFC_BASE_ADDR_AXI
#define CONFIG_MXC_NAND_IP_REGS_BASE	NFC_BASE_ADDR
#define CONFIG_MXC_NAND_HWECC
#define CONFIG_SYS_NAND_MAX_CHIPS	0x1
#define CONFIG_SYS_MAX_NAND_DEVICE	0x1
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#ifdef CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_OFFSET		CONFIG_U_BOOT_IMG_SIZE
#define CONFIG_ENV_SIZE			0x20000 /* 128 KiB */
#define CONFIG_ENV_RANGE		0x60000
#endif
#define CONFIG_SYS_NAND_BASE		0x00000000
#endif /* CONFIG_CMD_NAND */

/*
 * MMC Driver
 */
#ifdef CONFIG_FSL_ESDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR	0

#define CONFIG_CMD_FAT
#define CONFIG_FAT_WRITE
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
	"(env2),6m(linux),32m(rootfs),89344k(userfs),512k@0x7f00000(dtb),512k@0x7f80000(bbt)ro"
#else
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"1m(u-boot),"							\
	xstr(CONFIG_ENV_RANGE)						\
	"(env),6m(linux),32m(rootfs),89728k(userfs),512k@0x7f00000(dtb),512k@0x7f80000(bbt)ro"
#endif

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x1000 - /* Fix this */ \
					GENERATED_GBL_DATA_SIZE)

#ifdef CONFIG_CMD_IIM
#define CONFIG_FSL_IIM
#endif

#endif /* __CONFIG_H */
