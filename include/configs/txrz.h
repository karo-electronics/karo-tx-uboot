/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2015 Renesas Electronics Corporation
 */

#ifndef __SMARC_RZG2L_H
#define __SMARC_RZG2L_H

#include <asm/arch/rmobile.h>
#include <linux/sizes.h>

#define CONFIG_REMAKE_ELF

#ifdef CONFIG_SPL
#define CONFIG_SPL_TARGET	"spl/u-boot-spl.scif"
#endif

/* boot option */

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

/* Generic Interrupt Controller Definitions */
/* RZ/G2L use GIC-v3 */
#define CONFIG_GICV3
#define GICD_BASE	0x11900000
#define GICR_BASE	0x11960000

/* console */
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_BAUDRATE_TABLE	{ 115200, 38400 }

/* PHY needs a longer autoneg timeout */
#define PHY_ANEG_TIMEOUT		20000

/* MEMORY */
#define CONFIG_SYS_INIT_SP_ADDR		CONFIG_SYS_TEXT_BASE

/* SDHI clock freq */
#define CONFIG_SH_SDHI_FREQ		133000000

#define DRAM_RSV_SIZE			SZ_128M
#define CONFIG_SYS_SDRAM_BASE		(0x40000000 + DRAM_RSV_SIZE)

#if defined(CONFIG_KARO_TXRZ_G2L0) || defined(CONFIG_KARO_QSRZ_G2L0)
#define CONFIG_SYS_SDRAM_SIZE		(SZ_1G - DRAM_RSV_SIZE) //total 1GB
#elif defined(CONFIG_KARO_TXRZ_G2L1) || defined(CONFIG_KARO_QSRZ_G2L1)
#define CONFIG_SYS_SDRAM_SIZE		(SZ_512M - DRAM_RSV_SIZE) //total 512MB
#endif
#define CONFIG_SYS_LOAD_ADDR		0x58000000
#define CONFIG_LOADADDR			CONFIG_SYS_LOAD_ADDR // Default load address for tfpt,bootp...
#define CONFIG_VERY_BIG_RAM
#define CONFIG_MAX_MEM_MAPPED		CONFIG_SYS_SDRAM_SIZE

#define CONFIG_SYS_MONITOR_BASE		0x00000000
#define CONFIG_SYS_MONITOR_LEN		(1 * 1024 * 1024)
#define CONFIG_SYS_MALLOC_LEN		(64 * 1024 * 1024)
#define CONFIG_SYS_BOOTM_LEN		(64 << 20)

/* The HF/QSPI layout permits up to 1 MiB large bootloader blob */
#define CONFIG_BOARD_SIZE_LIMIT		1048576

/* For board */
/* Ethernet RAVB */
#define CONFIG_BITBANGMII_MULTI

#endif /* __SMARC_RZG2L_H */
