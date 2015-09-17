/*
 * Copyright (C) 2015 <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:      GPL-2.0
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/kconfig.h>
#include <linux/sizes.h>
#include <asm/hardware.h>

#define CONFIG_SYS_TEXT_BASE		0x20100000

/* ARM asynchronous clock */
#define CONFIG_SYS_AT91_SLOW_CLOCK	32768
#define CONFIG_SYS_AT91_MAIN_CLOCK	12000000 /* from 12 MHz crystal */

#define CONFIG_ARCH_CPU_INIT

#define CONFIG_SKIP_LOWLEVEL_INIT
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_DISPLAY_CPUINFO

#define CONFIG_SYS_GENERIC_BOARD

/* general purpose I/O */
#define CONFIG_AT91_GPIO

/* serial console */
#define CONFIG_ATMEL_USART
#define CONFIG_USART_BASE		ATMEL_BASE_USART0
#define CONFIG_USART_ID			ATMEL_ID_USART0
#define CONFIG_SYS_CONSOLE_INFO_QUIET

#define xstr(s)				str(s)
#define str(s)				#s
#define __pfx(x, s)			(x##s)
#define _pfx(x, s)			__pfx(x, s)

/* SDRAM */
#define CONFIG_NR_DRAM_BANKS		1
#define CONFIG_SYS_SDRAM_BASE		ATMEL_BASE_DDRCS
#define CONFIG_SYS_SDRAM_SIZE		0x20000000

#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_SDRAM_BASE + SZ_4K - GENERATED_GBL_DATA_SIZE)

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_BOOTDELAY		3
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_SYS_AUTOLOAD		"no"
#define CONFIG_BOOTFILE			"uImage"
#define CONFIG_BOOTARGS			"init=/linuxrc console=ttyS0,115200 ro debug panic=1"
#define CONFIG_BOOTCOMMAND		"run bootcmd_${boot_mode} bootm_cmd"
#define CONFIG_LOADADDR			22000000 /* load address */
#define CONFIG_FDTADDR			21000000
#define CONFIG_SYS_LOAD_ADDR		_pfx(0x, CONFIG_LOADADDR)
#define CONFIG_SYS_FDT_ADDR		_pfx(0x, CONFIG_FDTADDR)
#define CONFIG_U_BOOT_IMG_SIZE		SZ_1M

/* NAND flash */
#ifdef CONFIG_TXA5_NAND
#define CONFIG_SYS_MAX_NAND_DEVICE	0x1
/* our ALE is AD21 */
#define CONFIG_SYS_NAND_MASK_ALE	(1 << 21)
/* our CLE is AD22 */
#define CONFIG_SYS_NAND_MASK_CLE	(1 << 22)
#define CONFIG_SYS_NAND_ONFI_DETECTION
/* PMECC & PMERRLOC */
#define CONFIG_ATMEL_NAND_HWECC
#define CONFIG_ATMEL_NAND_HW_PMECC
#ifdef CONFIG_ENV_IS_IN_NAND
#define CONFIG_SYS_NAND_U_BOOT_OFFS	SZ_128K
#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_OFFSET		(CONFIG_U_BOOT_IMG_SIZE + CONFIG_SYS_NAND_U_BOOT_OFFS)
#define CONFIG_ENV_SIZE			SZ_128K
#define CONFIG_ENV_RANGE		0x60000
#endif /* CONFIG_ENV_IS_IN_NAND */
#define CONFIG_SYS_NAND_BASE		ATMEL_BASE_CS3
#define CONFIG_SYS_NAND_SIZE		SZ_128M
#define NAND_BASE			CONFIG_SYS_NAND_BASE
#endif /* CONFIG_CMD_NAND */

/* MMC */
#ifdef CONFIG_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_GENERIC_ATMEL_MCI
#define ATMEL_BASE_MMCI			ATMEL_BASE_MCI1

#define CONFIG_CMD_FAT
#define CONFIG_FAT_WRITE
#define CONFIG_CMD_EXT2
#endif

/* Ethernet Hardware */
#define CONFIG_MACB
#define CONFIG_RMII
#define CONFIG_NET_RETRY_COUNT		20
#define CONFIG_MACB_SEARCH_PHY

/* LCD */
/* #define CONFIG_LCD */
#ifdef CONFIG_LCD
#ifndef __ASSEMBLY__
extern int lcd_output_bpp;
#endif
#define LCD_BPP				LCD_COLOR16
#define LCD_OUTPUT_BPP			lcd_output_bpp
#define CONFIG_LCD_LOGO
#define CONFIG_ATMEL_HLCD
#define CONFIG_ATMEL_LCD_RGB565
#define CONFIG_CMD_BMP
#define CONFIG_VIDEO_BMP_RLE8
#endif

#ifdef CONFIG_TXA5_NAND
#define CONFIG_SYS_DEFAULT_BOOT_MODE	"nand"
#define CONFIG_SYS_BOOT_CMD_NAND					\
	"bootcmd_nand=setenv autostart no;run bootargs_ubifs;nboot linux\0"
#define CONFIG_SYS_FDTSAVE_CMD						\
	"fdtsave=fdt resize;nand erase.part dtb"			\
	";nand write ${fdtaddr} dtb ${fdtsize}\0"
#define MTD_NAME			"atmel_nand"
#define MTDIDS_DEFAULT			"nand0=" MTD_NAME
#ifdef CONFIG_ENV_OFFSET_REDUND
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"128k(u-boot-spl),"						\
	"1m(u-boot),"							\
	xstr(CONFIG_ENV_RANGE)						\
	"(env),"							\
	xstr(CONFIG_ENV_RANGE)						\
	"(env2),6m(linux),32m(rootfs),89216k(userfs),512k@0x7f00000(dtb),512k@0x7f80000(bbt)ro"
#else
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":"	\
	"128k(u-boot-spl),"						\
	"1m(u-boot),"							\
	xstr(CONFIG_ENV_RANGE)						\
	"(env),6m(linux),32m(rootfs),89600k(userfs),512k@0x7f00000(dtb),512k@0x7f80000(bbt)ro"
#endif

#define MMC_ROOT_STR " root=/dev/mmcblk0p2 rootwait\0"
#define ROOTPART_UUID_STR ""

#else /* CONFIG_TXA5_NAND */
/* bootstrap + u-boot + env in sd card */
#define CONFIG_SYS_FDTSAVE_CMD						\
	"fdtsave=mmc open 0 1;mmc write ${fdtaddr} "			\
	xstr(CONFIG_SYS_DTB_BLKNO) " 80;mmc close 0 1\0"
#define MMC_ROOT_STR " root=PARTUUID=${rootpart_uuid} rootwait\0"
#define ROOTPART_UUID_STR "rootpart_uuid=0cc66cc0-02\0"
#define MTD_NAME			""
#define MTDIDS_DEFAULT			""
#define MTDPARTS_DEFAULT		""
#define CONFIG_SYS_DEFAULT_BOOT_MODE	"mmc"
#define CONFIG_SYS_BOOT_CMD_NAND	""
#define CONFIG_SYS_FDTSAVE_CMD						\
	"fdtsave=mmc open 0 1;mmc write ${fdtaddr} "			\
	xstr(CONFIG_SYS_DTB_BLKNO) " 80;mmc close 0 1\0"
#define MMC_ROOT_STR " root=PARTUUID=${rootpart_uuid} rootwait\0"
#define ROOTPART_UUID_STR "rootpart_uuid=0cc66cc0-02\0"
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_ENV_SIZE			SZ_128K
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_SYS_MMC_ENV_PART		0x1
#define CONFIG_DYNAMIC_MMC_DEVNO
#endif /* CONFIG_ENV_IS_IN_MMC */
#endif /* CONFIG_TXA5_NAND */

/*
 * Extra Environment Settings
 */
#ifdef CONFIG_ENV_IS_NOWHERE
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"autoload=no\0"							\
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
	" root=/dev/mtdblock4 rootfstype=jffs2\0"			\
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
	"default_bootargs=setenv bootargs " CONFIG_BOOTARGS		\
	" ${append_bootargs}\0"						\
	"fdtaddr=" xstr(CONFIG_FDTADDR) "\0"				\
	CONFIG_SYS_FDTSAVE_CMD						\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"				\
	"nfsroot=/tftpboot/rootfs\0"					\
	"otg_mode=device\0"						\
	ROOTPART_UUID_STR						\
	"touchpanel=tsc2007\0"						\
	"video_mode=VGA\0"
#endif /*  CONFIG_ENV_IS_NOWHERE */


#define CONFIG_BAUDRATE			115200

#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_PBSIZE \
	(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_MAXARGS		256
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE

#define CONFIG_VERSION_VARIABLE		/* U-BOOT version */
#define CONFIG_AUTO_COMPLETE
#define CONFIG_CMDLINE_EDITING

#define CONFIG_SYS_64BIT_VSPRINTF

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		SZ_4M
#define CONFIG_SYS_MEMTEST_START	CONFIG_SYS_SDRAM_BASE	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + SZ_4M)

#endif
