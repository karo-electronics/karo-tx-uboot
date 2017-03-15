/*
 * Board configuration file for TXSD-410E
 *
 * (C) Copyright 2016  Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIGS_TXSD_H
#define __CONFIGS_TXSD_H

#include <linux/sizes.h>
#include <asm/arch/sysmap-apq8016.h>

#define CONFIG_ARMV8_PSCI
#define CONFIG_MISC_INIT_R
#define CONFIG_BOARD_LATE_INIT

/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS		1
#define CONFIG_SYS_TEXT_BASE		0x90080000
#define CONFIG_SYS_SDRAM_BASE		0x90000000
#define PHYS_SDRAM_1			CONFIG_SYS_SDRAM_BASE
#define PHYS_SDRAM_1_SIZE		(3 * SZ_256M)

/* 986 MiB (the last 38MiB are secured for TrustZone by ATF */
#define CONFIG_SYS_MEM_RESERVE_SECURE	(38 * SZ_1M)
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + 0x7fff0)

#define CONFIG_SYS_LOAD_ADDR		_pfx(0x, CONFIG_LOADADDR)
#define CONFIG_SYS_FDT_ADDR		_pfx(0x, CONFIG_FDTADDR)
#define CONFIG_SYS_RD_ADDR		_pfx(0x, CONFIG_RDADDR)

#define CONFIG_SYS_MEMTEST_START	PHYS_SDRAM_1
#define CONFIG_SYS_MEMTEST_END		(PHYS_SDRAM_1 + SZ_512M)

#define CONFIG_SYS_CACHELINE_SIZE	64

/* UART */
#define CONFIG_BAUDRATE			115200

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY		19000000

/* This are needed to have proper mmc support */
#define CONFIG_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_SDHCI

#define CONFIG_SYS_LDSCRIPT		"board/$(BOARDDIR)/u-boot.lds"

/*
 * Fixup - in init code we switch from device to host mode,
 * it has to be done after each HCD reset
 */
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET

#define CONFIG_USB_HOST_ETHER /* Enable USB Networking */

/* Support all relevant USB ethernet dongles */
#define CONFIG_USB_ETHER_DM9601
#define CONFIG_USB_ETHER_ASIX
#define CONFIG_USB_ETHER_ASIX88179
#define CONFIG_USB_ETHER_MCS7830
#define CONFIG_USB_ETHER_RTL8152
#define CONFIG_USB_ETHER_SMSC95XX

/* LCD Logo and Splash screen support */
#define CONFIG_LCD

#ifdef CONFIG_LCD
#define CONFIG_SYS_LVDS_IF
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN

#define CONFIG_LCD_LOGO
#define LCD_BPP				LCD_COLOR32
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP
#define CONFIG_VIDEO_BMP_RLE8
#endif /* CONFIG_LCD */

/* Extra Commands */
#define CONFIG_CMD_ENV
#define CONFIG_CMD_GPT
#define CONFIG_CMD_PART
#define CONFIG_CMD_TFTP
#define CONFIG_FAT_WRITE

/* Enable that for switching of boot partitions */
#define CONFIG_SUPPORT_EMMC_BOOT

/* Partition table support */
#define HAVE_BLOCK_DEVICE /* Needed for partition commands */
#define CONFIG_PARTITION_UUIDS

#define CONFIG_BOOTP_DNS
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_SUBNETMASK

#define CONFIG_CMDLINE_EDITING
#define CONFIG_AUTO_COMPLETE
#define CONFIG_SYS_LONGHELP
#define CONFIG_DOS_PARTITION
#define CONFIG_EFI_PARTITION
#define CONFIG_SUPPORT_RAW_INITRD
#define CONFIG_SHOW_ACTIVITY

#define xstr(s)				str(s)
#define str(s)				#s
#define __pfx(x, s)			(x##s)
#define _pfx(x, s)			__pfx(x, s)

/* BOOTP options */
#define CONFIG_BOOTP_BOOTFILESIZE

/* Environment - Boot*/

#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION 1
#define EMMC_BOOT_ACK_STR		"emmc_boot_ack=1\0"
#define EMMC_BOOT_PART_STR		"emmc_boot_part="	\
	xstr(CONFIG_SYS_MMCSD_FS_BOOT_PARTITION) "\0"

#define CONFIG_BOOTCOMMAND		"run bootcmd_${boot_mode} bootm_cmd"
#define CONFIG_SYS_AUTOLOAD		"no"

/* Environment */
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"autostart=no\0"						\
	"baseboard=mb7\0"						\
	"bootargs_mmc=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=PARTUUID=${rootpart_uuid} rootwait\0"			\
	"bootargs_nfs=run default_bootargs;setenv bootargs ${bootargs}"	\
	" root=/dev/nfs nfsroot=${nfs_server}:${nfsroot},nolock"	\
	" ip=dhcp\0"							\
	"bootcmd_mmc=setenv autostart no;run bootargs_mmc"		\
	";load mmc ${bootpart} ${loadaddr} Image.gz\0"			\
	"bootcmd_net=setenv autoload y;setenv autostart n"		\
	";run bootargs_nfs;dhcp\0"					\
	"bootcmd_nfs=setenv autoload y;setenv autostart n"		\
	";run bootargs_nfs;load mmc ${bootpart} ${loadaddr} Image.gz\0"	\
	"bootdelay=1\0"							\
	"bootm_cmd=booti ${loadaddr} - ${fdtaddr}\0"			\
	"boot_mode=mmc\0"						\
	"bootpart=0:4\0"						\
	"default_bootargs=setenv bootargs init=/linuxrc"		\
	" console=ttyMSM0,115200n8 panic=1 ${append_bootargs}\0"	\
	EMMC_BOOT_PART_STR						\
	EMMC_BOOT_ACK_STR						\
	"fdtaddr=" xstr(CONFIG_FDTADDR) "\0"				\
	"fdtcontroladdr=" xstr(CONFIG_FDTADDR) "\0"			\
	"fdtsave=fdt resize;setexpr fs ${fs} / 200"			\
	";mmc write ${fdtaddr} c22 ${fs}\0"				\
	"fdt_high=0xffffffffffffffff\0"					\
	"initrd_high=0xffffffffffffffff\0"				\
	"nfsroot=/tftpboot/rootfs\0"					\
	"otg_mode=device\0"						\
	"rdaddr=" xstr(CONFIG_RDADDR) "\0"				\
	"rootpart_uuid=7f63ea01-808e-ad2c-6a4b-8f386a2bd218\0"		\
	"touchpanel=edt-ft5x06\0"					\
	"video_mode=VGA\0"

#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_SYS_MMC_ENV_PART		0x0
#define CONFIG_ENV_VARS_UBOOT_CONFIG
#define CONFIG_ENV_OVERWRITE

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE + SZ_64M)

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE		512	/* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
#define CONFIG_SYS_MAXARGS		64	/* max command args */
#define CONFIG_CMDLINE_EDITING

#endif /* __CONFIGS_TXSD_H */
