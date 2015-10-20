/*
 * Copyright (C) 2012-2015 <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:      GPL-2.0
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifndef CONFIG_SOC_MX6UL
#define CONFIG_ARM_ERRATA_743622
#define CONFIG_ARM_ERRATA_751472
#define CONFIG_ARM_ERRATA_794072
#define CONFIG_ARM_ERRATA_761320

#ifndef CONFIG_SYS_L2CACHE_OFF
#define CONFIG_SYS_L2_PL310
#define CONFIG_SYS_PL310_BASE   L2_PL310_BASE
#endif

#define CONFIG_SYS_FSL_ESDHC_HAS_DDR_MODE
#define CONFIG_MP
#endif
#define CONFIG_BOARD_POSTCLK_INIT
#define CONFIG_MXC_GPT_HCLK

#include <linux/kconfig.h>
#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

/*
 * Ka-Ro TX6 board - SoC configuration
 */
#define CONFIG_SYS_MX6_HCLK		24000000
#define CONFIG_SYS_MX6_CLK32		32768
#define CONFIG_SYS_HZ			1000		/* Ticks per second */
#define CONFIG_SHOW_ACTIVITY
#define CONFIG_ARCH_CPU_INIT
#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_SYS_GENERIC_BOARD
#define CONFIG_CMD_GPIO

#ifndef CONFIG_TX6_UBOOT_MFG
/* LCD Logo and Splash screen support */
#ifdef CONFIG_LCD
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#ifndef CONFIG_SOC_MX6UL
#define CONFIG_VIDEO_IPUV3
#define CONFIG_IPUV3_CLK		(CONFIG_SYS_SDRAM_CLK * 1000000 / 2)
#endif
#define CONFIG_LCD_LOGO
#define LCD_BPP				LCD_COLOR32
#define CONFIG_CMD_BMP
#define CONFIG_VIDEO_BMP_RLE8
#endif /* CONFIG_LCD */
#endif /* CONFIG_TX6_UBOOT_MFG */

/*
 * Memory configuration options
 */
#define CONFIG_NR_DRAM_BANKS		0x1		/* # of SDRAM banks */
#ifndef CONFIG_SOC_MX6UL
#define PHYS_SDRAM_1			0x10000000	/* Base address of bank 1 */
#define CONFIG_SYS_MPU_CLK		792
#else
#define PHYS_SDRAM_1			0x80000000	/* Base address of bank 1 */
#define CONFIG_SYS_MPU_CLK		528
#endif
#ifndef CONFIG_SYS_SDRAM_BUS_WIDTH
#if defined(CONFIG_SYS_SDRAM_BUS_WIDTH_32)
#define CONFIG_SYS_SDRAM_BUS_WIDTH	32
#elif defined(CONFIG_SYS_SDRAM_BUS_WIDTH_16)
#define CONFIG_SYS_SDRAM_BUS_WIDTH	16
#else
#define CONFIG_SYS_SDRAM_BUS_WIDTH	64
#endif
#endif /* CONFIG_SYS_SDRAM_BUS_WIDTH */
#define PHYS_SDRAM_1_SIZE		(SZ_512M / 32 * CONFIG_SYS_SDRAM_BUS_WIDTH)
#ifdef CONFIG_SOC_MX6Q
#define CONFIG_SYS_SDRAM_CLK		528
#else
#define CONFIG_SYS_SDRAM_CLK		400
#endif
#define CONFIG_STACKSIZE		SZ_128K
#define CONFIG_SPL_STACK		(IRAM_BASE_ADDR + SZ_16K)
#define CONFIG_SYS_MALLOC_LEN		SZ_8M
#define CONFIG_SYS_MEMTEST_START	PHYS_SDRAM_1	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + SZ_4M)

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#if defined(CONFIG_SOC_MX6Q)
#elif defined(CONFIG_SOC_MX6DL)
#elif defined(CONFIG_SOC_MX6S)
#elif defined(CONFIG_SOC_MX6UL)
#else
#error Unsupported i.MX6 processor variant
#endif
#define CONFIG_SYS_CBSIZE		2048	/* Console I/O buffer size */
#define CONFIG_SYS_PBSIZE						\
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
 * Boot Linux
 */
#define xstr(s)				str(s)
#define str(s)				#s
#define __pfx(x, s)			(x##s)
#define _pfx(x, s)			__pfx(x, s)

#define CONFIG_CMDLINE_TAG
#define CONFIG_INITRD_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#ifndef CONFIG_TX6_UBOOT_MFG
#define CONFIG_BOOTDELAY		1
#else
#define CONFIG_BOOTDELAY		0
#endif
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_SYS_AUTOLOAD		"no"
#define DEFAULT_BOOTCMD			"run bootcmd_${boot_mode} bootm_cmd"
#define CONFIG_BOOTFILE			"uImage"
#define CONFIG_BOOTARGS			"init=/linuxrc console=ttymxc0,115200 ro debug panic=1"
#ifndef CONFIG_TX6_UBOOT_MFG
#define CONFIG_BOOTCOMMAND		DEFAULT_BOOTCMD
#else
#define CONFIG_BOOTCOMMAND		"setenv bootcmd '" DEFAULT_BOOTCMD "';" \
	"env import " xstr(CONFIG_BOOTCMD_MFG_LOADADDR) ";run bootcmd_mfg"
#if (defined(CONFIG_SOC_MX6SX) || defined(CONFIG_SOC_MX6SL) || defined(CONFIG_SOC_MX6UL))
#define CONFIG_BOOTCMD_MFG_LOADADDR	80500000
#else
#define CONFIG_BOOTCMD_MFG_LOADADDR	10500000
#endif
#define CONFIG_DELAY_ENVIRONMENT
#endif /* CONFIG_TX6_UBOOT_MFG */
#if (defined(CONFIG_SOC_MX6SX) || defined(CONFIG_SOC_MX6SL) || defined(CONFIG_SOC_MX6UL))
#define CONFIG_LOADADDR			82000000
#define CONFIG_FDTADDR			81000000
#else
#define CONFIG_LOADADDR			18000000
#define CONFIG_FDTADDR			11000000
#endif
#define CONFIG_SYS_LOAD_ADDR		_pfx(0x, CONFIG_LOADADDR)
#define CONFIG_SYS_FDT_ADDR		_pfx(0x, CONFIG_FDTADDR)
#ifndef CONFIG_SYS_LVDS_IF
#define DEFAULT_VIDEO_MODE		"VGA"
#else
#define DEFAULT_VIDEO_MODE		"HSD100PXN1"
#endif

/*
 * Extra Environments
 */
#ifdef CONFIG_TX6_UBOOT_NOENV
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
	MMC_ROOT_STR							\
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
	CONFIG_SYS_BOOT_CMD_NAND					\
	"bootcmd_net=setenv autoload y;setenv autostart n"		\
	";run bootargs_nfs"						\
	";dhcp\0"							\
	"bootm_cmd=bootm ${loadaddr} - ${fdtaddr}\0"			\
	"boot_mode=" CONFIG_SYS_DEFAULT_BOOT_MODE "\0"			\
	"cpu_clk=" CONFIG_SYS_CPU_CLK_STR "\0"				\
	"default_bootargs=setenv bootargs " CONFIG_BOOTARGS		\
	" ${append_bootargs}\0"						\
	EMMC_BOOT_PART_STR						\
	EMMC_BOOT_ACK_STR						\
	"fdtaddr=" xstr(CONFIG_FDTADDR) "\0"				\
	CONFIG_SYS_FDTSAVE_CMD						\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"				\
	"nfsroot=/tftpboot/rootfs\0"					\
	"otg_mode=device\0"						\
	ROOTPART_UUID_STR						\
	"touchpanel=tsc2007\0"						\
	"video_mode=" DEFAULT_VIDEO_MODE "\0"
#endif /*  CONFIG_ENV_IS_NOWHERE */

#ifdef CONFIG_TX6_NAND
#define CONFIG_SYS_DEFAULT_BOOT_MODE	"nand"
#define CONFIG_SYS_BOOT_CMD_NAND					\
	"bootcmd_nand=setenv autostart no;run bootargs_ubifs;nboot linux\0"
#define CONFIG_SYS_FDTSAVE_CMD						\
	"fdtsave=fdt resize;nand erase.part dtb"			\
	";nand write ${fdtaddr} dtb ${fdtsize}\0"
#define MTD_NAME			"gpmi-nand"
#define MTDIDS_DEFAULT			"nand0=" MTD_NAME
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define MMC_ROOT_STR " root=/dev/mmcblk0p2 rootwait\0"
#define ROOTPART_UUID_STR		""
#define EMMC_BOOT_ACK_STR		""
#define EMMC_BOOT_PART_STR		""
#else
#define CONFIG_SYS_DEFAULT_BOOT_MODE	"mmc"
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION 1
#define CONFIG_SYS_BOOT_CMD_NAND	""
#define CONFIG_SYS_FDTSAVE_CMD						\
	"fdtsave=mmc partconf 0 ${emmc_boot_ack} ${emmc_boot_part} ${emmc_boot_part}" \
	";mmc write ${fdtaddr} " xstr(CONFIG_SYS_DTB_BLKNO) " 80"	\
	";mmc partconf 0 ${emmc_boot_ack} ${emmc_boot_part} 0\0"
#define MTD_NAME			""
#define MTDIDS_DEFAULT			""
#define MMC_ROOT_STR " root=PARTUUID=${rootpart_uuid} rootwait\0"
#define ROOTPART_UUID_STR "rootpart_uuid=0cc66cc0-02\0"
#define EMMC_BOOT_ACK_STR		"emmc_boot_ack=1\0"
#define EMMC_BOOT_PART_STR		"emmc_boot_part="	\
	xstr(CONFIG_SYS_MMCSD_FS_BOOT_PARTITION) "\0"
#endif /* CONFIG_TX6_NAND */

/*
 * Serial Driver
 */
#define CONFIG_MXC_UART
#define CONFIG_MXC_UART_BASE		UART1_BASE
#define CONFIG_BAUDRATE			115200		/* Default baud rate */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200, }
#define CONFIG_SYS_CONSOLE_INFO_QUIET
#define CONFIG_CONS_INDEX		1

/*
 * GPIO driver
 */
#define CONFIG_MXC_GPIO

/*
 * Ethernet Driver
 */
#ifdef CONFIG_FEC_MXC
/* This is required for the FEC driver to work with cache enabled */
#define CONFIG_SYS_ARM_CACHE_WRITETHROUGH

#ifndef CONFIG_SOC_MX6UL
#define CONFIG_FEC_MXC_PHYADDR		0
#define IMX_FEC_BASE			ENET_BASE_ADDR
#endif
#define CONFIG_FEC_XCV_TYPE		RMII
#endif

/*
 * I2C Configs
 */
#ifdef CONFIG_HARD_I2C
#define CONFIG_SYS_I2C_BASE		I2C1_BASE_ADDR
#define CONFIG_SYS_I2C_SPEED		400000
#if defined(CONFIG_TX6_REV)
#if CONFIG_TX6_REV == 0x1
#define CONFIG_SYS_I2C_SLAVE		0x3c
#define CONFIG_LTC3676
#elif CONFIG_TX6_REV == 0x2
#define CONFIG_SYS_I2C_SLAVE		0x32
#define CONFIG_RN5T618
#elif CONFIG_TX6_REV == 0x3
#define CONFIG_SYS_I2C_SLAVE		0x33
#define CONFIG_RN5T567
#else
#error Unsupported TX6 module revision
#endif
#endif /* CONFIG_TX6_REV */
/* autodetect which PMIC is present to derive TX6_REV */
#ifndef CONFIG_SOC_MX6UL
#define CONFIG_LTC3676			/* TX6_REV == 1 */
#endif
#define CONFIG_RN5T567			/* TX6_REV == 3 */
#endif /* CONFIG_HARD_I2C */

#define CONFIG_ENV_OVERWRITE

/*
 * NAND flash driver
 */
#ifdef CONFIG_TX6_NAND
#define CONFIG_SYS_MXS_DMA_CHANNEL	4
#define CONFIG_SYS_MAX_FLASH_BANKS	0x1
#define CONFIG_SYS_NAND_MAX_CHIPS	0x1
#define CONFIG_SYS_MAX_NAND_DEVICE	0x1
#define CONFIG_SYS_NAND_BASE		0x00000000
#define CONFIG_SYS_NAND_5_ADDR_CYCLE

#define CONFIG_ENV_OFFSET		(CONFIG_U_BOOT_IMG_SIZE + CONFIG_SYS_NAND_U_BOOT_OFFS)
#define CONFIG_ENV_SIZE			SZ_128K
#define CONFIG_ENV_RANGE		(3 * CONFIG_SYS_NAND_BLOCK_SIZE)
#endif /* CONFIG_TX6_NAND */

#ifdef CONFIG_ENV_OFFSET_REDUND
#define CONFIG_SYS_ENV_PART_STR		xstr(CONFIG_SYS_ENV_PART_SIZE)	\
	"(env),"							\
	xstr(CONFIG_SYS_ENV_PART_SIZE)					\
	"(env2),"
#define CONFIG_SYS_USERFS_PART_STR	xstr(CONFIG_SYS_USERFS_PART_SIZE) "(userfs)"
#else
#define CONFIG_SYS_ENV_PART_STR		xstr(CONFIG_SYS_ENV_PART_SIZE)	\
	"(env),"
#define CONFIG_SYS_USERFS_PART_STR	xstr(CONFIG_SYS_USERFS_PART_SIZE2) "(userfs)"
#endif /* CONFIG_ENV_OFFSET_REDUND */

/*
 * MMC Driver
 */
#ifdef CONFIG_FSL_ESDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR	0
#endif
#ifdef CONFIG_CMD_MMC
#define CONFIG_CMD_FAT
#define CONFIG_FAT_WRITE
#define CONFIG_CMD_EXT2

/*
 * Environments on MMC
 */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_SYS_MMC_ENV_PART		0x1
#define CONFIG_DYNAMIC_MMC_DEVNO
#endif /* CONFIG_ENV_IS_IN_MMC */
#endif /* CONFIG_CMD_MMC */

#ifdef CONFIG_TX6_NAND
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	xstr(CONFIG_SYS_U_BOOT_PART_SIZE)				\
	"@" xstr(CONFIG_SYS_NAND_U_BOOT_OFFS)				\
	"(u-boot),"							\
	CONFIG_SYS_ENV_PART_STR						\
	"6m(linux),32m(rootfs)," CONFIG_SYS_USERFS_PART_STR ","		\
	xstr(CONFIG_SYS_DTB_PART_SIZE)					\
	"@" xstr(CONFIG_SYS_NAND_DTB_OFFSET) "(dtb),"			\
	xstr(CONFIG_SYS_NAND_BBT_SIZE)					\
	"@" xstr(CONFIG_SYS_NAND_BBT_OFFSET) "(bbt)ro"
#else
#define MTDPARTS_DEFAULT		""
#endif

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x1000 - /* Fix this */ \
					GENERATED_GBL_DATA_SIZE)

#endif /* __CONFIG_H */
