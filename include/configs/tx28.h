/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef __CONFIG_H
#define __CONFIG_H

#include <asm/sizes.h>
#include <asm/arch/regs-base.h>

/*
 * Ka-Ro TX28 board - SoC configuration
 */
#define CONFIG_MX28				/* i.MX28 SoC */
#define	CONFIG_MXS_GPIO				/* GPIO control */
#define CONFIG_SYS_HZ		1000		/* Ticks per second */
#define CONFIG_IDENT_STRING	"\nBoard: Ka-Ro TX28-40xx"
#define CONFIG_SHOW_ACTIVITY

#define CONFIG_SPL
#define CONFIG_SPL_NO_CPU_SUPPORT_CODE
#define CONFIG_SPL_START_S_PATH	"arch/arm/cpu/arm926ejs/mx28"
#define CONFIG_SPL_LDSCRIPT	"arch/arm/cpu/arm926ejs/mx28/u-boot-spl.lds"
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_GPIO_SUPPORT
#define CONFIG_SYS_SPL_FIXED_BATT_SUPPLY
#define CONFIG_SYS_SPL_VDDD_VAL		1500
#define CONFIG_SYS_SPL_BATT_BO_LEVEL	2800
#define CONFIG_SKIP_LOWLEVEL_INIT

/*
 * Memory configurations
 */
#define CONFIG_NR_DRAM_BANKS	1		/* 1 bank of DRAM */
#define PHYS_SDRAM_1		0x40000000	/* Base address */
#define PHYS_SDRAM_1_SIZE	SZ_128M
#define CONFIG_STACKSIZE	0x00010000	/* 128 KB stack */
#define CONFIG_SYS_MALLOC_LEN	0x00400000	/* 4 MB for malloc */
#define CONFIG_SYS_GBL_DATA_SIZE 128		/* Reserved for initial data */
#define CONFIG_SYS_MEMTEST_START 0x40000000	/* Memtest start address */
#define CONFIG_SYS_MEMTEST_END	 0x40400000	/* 4 MB RAM test */

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_PROMPT	"MX28 U-Boot > "
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

/*
 * Boot Linux
 */
#define xstr(s)	str(s)
#define str(s)	#s

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_BOOTDELAY	3
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_BOOTFILE		"uImage"
#define CONFIG_BOOTARGS		"console=ttyAMA0,115200 tx28_base=stkv3" \
	" tx28_otg_mode=device ro debug panic=1"
#define CONFIG_BOOTCOMMAND	"run bootcmd_nand"
#define CONFIG_LOADADDR		0x40100000
#define CONFIG_SYS_LOAD_ADDR	CONFIG_LOADADDR
#define CONFIG_BOARD_EARLY_INIT_F

/*
 * Extra Environments
 */
#define	CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"bootcmd_mmc=set autostart yes;run bootargs_mmc;"		\
	" mmc read ${loadaddr} 100 3000\0"				\
	"bootcmd_nand=set autostart yes;run bootargs_nand;"		\
	" nboot linux\0"						\
	"bootcmd_net=setenv autostart yes;run bootargs_nfs; dhcp\0"	\
	"bootargs_mmc=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=/dev/mmcblk0p3 rootwait\0"				\
	"bootargs_nand=run default_bootargs;setenv bootargs ${bootargs}"\
	" ${mtdparts} root=/dev/mtdblock3 rootfstype=jffs2\0"		\
	"nfsroot=/tftpboot/rootfs\0"					\
	"bootargs_nfs=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=/dev/nfs ip=dhcp nfsroot=${serverip}:${nfsroot},nolock\0"\
	"default_bootargs=set bootargs " CONFIG_BOOTARGS		\
	" mxsfb.mode=${video_mode}\0"					\
	"mtdids=" MTDIDS_DEFAULT "\0"					\
	"mtdparts=" MTDPARTS_DEFAULT "\0"				\
	"video_mode=VGA\0"

#define MTD_NAME			"gpmi-nand"
#define MTDIDS_DEFAULT			"nand0=" MTD_NAME

/*
 * U-Boot Commands
 */
#define CONFIG_SYS_NO_FLASH
#include <config_cmd_default.h>
#define CONFIG_ARCH_CPU_INIT
#define CONFIG_DISPLAY_CPUINFO

/*
 * Serial Driver
 */
#define	CONFIG_PL011_SERIAL
#define	CONFIG_PL011_CLOCK		24000000
#define	CONFIG_PL01x_PORTS		{ (void *)MXS_UARTDBG_BASE }
#define	CONFIG_CONS_INDEX		0
#define CONFIG_BAUDRATE			115200		/* Default baud rate */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

/*
 * FEC Driver
 */
#define CONFIG_FEC_MXC
#ifdef CONFIG_FEC_MXC
/* This is required for the FEC driver to work with cache enabled */
#define CONFIG_SYS_ARM_CACHE_WRITETHROUGH

#define CONFIG_FEC_MXC_MULTI

#define CONFIG_MII
#define CONFIG_FEC_XCV_TYPE		RMII
#define CONFIG_GET_FEC_MAC_ADDR_FROM_IIM
#define CONFIG_NET_MULTI
#define CONFIG_ETH_PRIME
#define CONFIG_CMD_MII
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PING
/* Add for working with "strict" DHCP server */
#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_DNS
#endif

#define CONFIG_CMD_MMC
#define CONFIG_CMD_NAND
#define CONFIG_CMD_MTDPARTS

/*
 * NAND flash driver
 */
#ifdef CONFIG_CMD_NAND
#define CONFIG_MTD_DEVICE
#define CONFIG_ENV_IS_IN_NAND
#define CONFIG_NAND_MXS
#define CONFIG_APBH_DMA
#define CONFIG_APBH_DMA_BURST
#define CONFIG_APBH_DMA_BURST8
#define CONFIG_CMD_NAND_TRIMFFS
#define CONFIG_SYS_MXS_DMA_CHANNEL	4
#define CONFIG_SYS_MAX_FLASH_SECT	1024
#define CONFIG_SYS_MAX_FLASH_BANKS	1
#define CONFIG_SYS_NAND_MAX_CHIPS	1
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define	CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_USE_FLASH_BBT
#ifdef CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_OFFSET		0x20000
#define CONFIG_ENV_SIZE			0x20000 /* 128 KiB */
#if 0
#define CONFIG_ENV_OFFSET_REDUND	0x40000
#define CONFIG_ENV_SIZE_REDUND		CONFIG_ENV_SIZE
#endif
#endif
#ifndef CONFIG_SYS_NO_FLASH
#define CONFIG_CMD_FLASH
#define CONFIG_SYS_NAND_BASE		0xa0000000
#define CONFIG_FIT
#define CONFIG_OF_LIBFDT
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
#define	CONFIG_GENERIC_MMC
#define	CONFIG_MXS_MMC
#define CONFIG_DOS_PARTITION
#define CONFIG_CMD_FAT

#define CONFIG_BOOT_PARTITION_ACCESS
#define CONFIG_DOS_PARTITION
#define CONFIG_CMD_FAT
#define CONFIG_CMD_EXT2

/*
 * Environments on MMC
 */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV	0
#define CONFIG_CMD_ENV
#define CONFIG_ENV_OVERWRITE
/* Assoiated with the MMC layout defined in mmcops.c */
#define CONFIG_ENV_OFFSET		0x400 /* 1 KB */
#define CONFIG_ENV_SIZE			(0x20000 - 0x400) /* 127 KB */
#define CONFIG_DYNAMIC_MMC_DEVNO
#endif /* CONFIG_ENV_IS_IN_MMC */
#endif /* CONFIG_CMD_MMC */

#ifdef CONFIG_ENV_OFFSET_REDUND
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":128k@"	\
	xstr(CONFIG_ENV_OFFSET)						\
	"(env),"							\
	xstr(CONFIG_ENV_OFFSET_REDUND)				\
	"(env2),1m(u-boot),4m(linux),16m(rootfs),-(userfs)"
#else
#define MTDPARTS_DEFAULT		"mtdparts=" MTD_NAME ":128k@"	\
	xstr(CONFIG_ENV_OFFSET)						\
	"(env),1m(u-boot),4m(linux),16m(rootfs),-(userfs)"
#endif

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM_1
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x1000 - /* Fix this */ \
					GENERATED_GBL_DATA_SIZE)

#endif /* __CONFIG_H */
