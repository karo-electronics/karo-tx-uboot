/*
 * (C) Copyright 2002
 * Kyle Harris, Nexus Technologies, Inc. kharris@nexus-tech.net
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * Configuation settings for the TRITON320 board.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_CPU_MONAHANS	1	/* Intel Monahan CPU    */
#define CONFIG_TRITON320	1	/* Zylonite board       */

/* #define CONFIG_LCD		1 */
#ifdef CONFIG_LCD
#define CONFIG_SHARP_LM8V31
#endif
/* #define CONFIG_MMC		1 */
#define BOARD_LATE_INIT		1

#define CONFIG_SKIP_RELOCATE_UBOOT	1
#undef CONFIG_SKIP_LOWLEVEL_INIT  
#undef CONFIG_USE_IRQ			/* we don't need IRQ/FIQ stuff */

/*
 * Size of malloc() pool
 */
#define CFG_MALLOC_LEN	    (CFG_ENV_SIZE + 256*1024)
#define CFG_GBL_DATA_SIZE	512	/* size in bytes reserved for initial data */

/*
 * Hardware drivers
 */


#define CONFIG_DRIVER_DM9000		1
#define CONFIG_DM9000_BASE		0x10000300
#define DM9000_IO			CONFIG_DM9000_BASE
#define DM9000_DATA			(CONFIG_DM9000_BASE+0x8000)
#define CONFIG_DM9000_USE_16BIT



/*
 * select serial console configuration
 */
#define CONFIG_FFUART	       1

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

#define CONFIG_BAUDRATE		38400

#if 0
# define CONFIG_COMMANDS	CFG_CMD_AUTOSCRIPT 	\
		|	CFG_CMD_BDI		\
		|	CFG_CMD_BOOTD		\
		|	CFG_CMD_CONSOLE		\
		|	CFG_CMD_ECHO		\
		|	CFG_CMD_ENV		\
		|	CFG_CMD_IMI		\
		|	CFG_CMD_ITEST		\
		|	CFG_CMD_LOADB		\
		|	CFG_CMD_LOADS		\
		|	CFG_CMD_MEMORY		\
		|	CFG_CMD_NAND		\
		|	CFG_CMD_REGINFO		\
		|	CFG_CMD_RUN	\
		&	~(CFG_CMD_JFFS2 | CFG_CMD_FLASH | CFG_CMD_IMLS)
#endif

#define CONFIG_COMMANDS	((CONFIG_CMD_DFL|	\
			 CFG_CMD_NAND	|	\
			 CFG_CMD_JFFS2	|	\
			 CFG_CMD_PING	|	\
			 CFG_CMD_DHCP)	&	\
			~(CFG_CMD_FLASH |	\
			  CFG_CMD_IMLS))

/* this must be included AFTER the definition of CONFIG_COMMANDS (if any) */
#include <cmd_confdefs.h>

#define CONFIG_BOOTDELAY			3
#define CONFIG_BOOTCOMMAND			"bootm 80000"
#define CONFIG_BOOTARGS				"root=/dev/mtdblock1 rootfstype=jffs2 console=ttyS0,38400"
#define CONFIG_BOOT_RETRY_TIME		-1
#define CONFIG_BOOT_RETRY_MIN		60
#define CONFIG_RESET_TO_RETRY
#define CONFIG_AUTOBOOT_PROMPT		"autoboot in %d seconds\n"
#define CONFIG_AUTOBOOT_DELAY_STR 	" "
#define CONFIG_AUTOBOOT_STOP_STR	"system"

#define CONFIG_ETHADDR			ff:ff:ff:ff:ff:ff
#define CONFIG_NETMASK			255.255.255.255
#define CONFIG_IPADDR			0.0.0.0
#define CONFIG_SERVERIP			0.0.0.0
#define CONFIG_CMDLINE_TAG		1
#define CONFIG_SETUP_MEMORY_TAGS 	1
#define CONFIG_CMDLINE_EDITING
#define CONFIG_TIMESTAMP
#define CONFIG_USE_MAC_FROM_ENV

#if (CONFIG_COMMANDS & CFG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	230400		/* speed to run kgdb serial port */
#define CONFIG_KGDB_SER_INDEX	2		/* which serial port to use */
#endif

/*
 * Miscellaneous configurable options
 */
#define CFG_HUSH_PARSER		1
#define CFG_PROMPT_HUSH_PS2	"> "

#define CFG_LONGHELP				/* undef to save memory		*/
#ifdef CFG_HUSH_PARSER
#define CFG_PROMPT		"$ "		/* Monitor Command Prompt */
#else
#define CFG_PROMPT		"=> "		/* Monitor Command Prompt */
#endif
#define CFG_CBSIZE		256		/* Console I/O Buffer Size	*/
#define CFG_PBSIZE (CFG_CBSIZE+sizeof(CFG_PROMPT)+16) /* Print Buffer Size */
#define CFG_MAXARGS		16		/* max number of command args	*/
#define CFG_BARGSIZE		CFG_CBSIZE	/* Boot Argument Buffer Size	*/
#define CFG_DEVICE_NULLDEV	1

#define CFG_MEMTEST_START	0x00400000	/* memtest works on	*/
#define CFG_MEMTEST_END		0x00800000	/* 4 ... 8 MB in DRAM	*/

#undef	CFG_CLKS_IN_HZ		/* everything, incl board info, in Hz */

#define CFG_HZ			3250000		/* incrementer freq: 3.25 MHz */

/* Monahans Core Frequency */
#define CFG_MONAHANS_RUN_MODE_OSC_RATIO		31 /* valid values: 8, 16, 24, 31 */
#define CFG_MONAHANS_TURBO_RUN_MODE_RATIO	2  /* valid values: 1, 2 */

						/* valid baudrates */
#define CFG_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

/* #define CFG_MMC_BASE		0xF0000000 */

/*
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128*1024)	/* regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)	/* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)	/* FIQ stack */
#endif

/*
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1	   /* we have 1 banks of DRAM */
#define PHYS_SDRAM_1		0x80000000 /* SDRAM Bank #1 */
#define PHYS_SDRAM_1_SIZE	0x04000000 /* 64 MB */

#define CFG_DRAM_BASE		PHYS_SDRAM_1
#define CFG_DRAM_SIZE		PHYS_SDRAM_1_SIZE


#define CFG_LOAD_ADDR		(PHYS_SDRAM_1 + 0x100000) /* default load address */

#define CFG_SKIP_DRAM_SCRUB

/*
 * NAND Flash
 */
/* Use the new NAND code. (BOARDLIBS = drivers/nand/libnand.a required) */
#define CONFIG_NEW_NAND_CODE
#define CFG_NAND0_BASE		0x0
#undef CFG_NAND1_BASE

#define CONFIG_MTD_NAND_ECC_JFFS2 1

#define CFG_NAND_BASE_LIST	{ CFG_NAND0_BASE }
#define CFG_MAX_NAND_DEVICE	1	/* Max number of NAND devices */

/* nand timeout values */
#define CFG_NAND_PROG_ERASE_TO	9000
#define CFG_NAND_OTHER_TO	2000
#define CFG_NAND_SENDCMD_RETRY	3
#undef NAND_ALLOW_ERASE_ALL	/* Allow erasing bad blocks - don't use */

/* NAND Timing Parameters (in ns) */
#define NAND_TIMING_tCH		10
#define NAND_TIMING_tCS		0
#define NAND_TIMING_tWH		20
#define NAND_TIMING_tWP		40

#define NAND_TIMING_tRH		20
#define NAND_TIMING_tRP		40

#define NAND_TIMING_tR		11123
#define NAND_TIMING_tWHR	100
#define NAND_TIMING_tAR		10

/* NAND debugging */
#if 0
#define  	CFG_DFC_DEBUG1  	/* useful */
#define	 	CFG_DFC_DEBUG2  	/* noisy */
#define  	CFG_DFC_DEBUG3   	/* extremly noisy  */
#else
#undef  	CFG_DFC_DEBUG1  	/* useful */
#undef	 	CFG_DFC_DEBUG2  	/* noisy */
#undef  	CFG_DFC_DEBUG3   	/* extremly noisy  */
#endif

#define CONFIG_MTD_DEBUG	0
#define CONFIG_MTD_DEBUG_VERBOSE 0

#define ADDR_COLUMN		1
#define ADDR_PAGE		2
#define ADDR_COLUMN_PAGE	3

#define NAND_ChipID_UNKNOWN	0x00
#define NAND_MAX_FLOORS		1
#define NAND_MAX_CHIPS		1

#define CFG_NO_FLASH		1

#define CFG_ENV_IS_IN_NAND	1
#define CFG_ENV_OFFSET		0x60000
#undef CFG_ENV_OFFSET_REDUND
#define CFG_ENV_SIZE		0x20000

#define CONFIG_JFFS2_NAND 1
#define CONFIG_JFFS2_NAND_DEV "nand0"			/* nand device jffs2 lives on */
#define CONFIG_JFFS2_NAND_OFF 0x80000			/* start of jffs2 partition */
#define CONFIG_JFFS2_NAND_SIZE 64*1024*1024		/* size of jffs2 partition */


/* mtdparts command line support */
#define CONFIG_JFFS2_CMDLINE
#define MTDIDS_DEFAULT		"nand0=triton320-nand"
#define MTDPARTS_DEFAULT	"mtdparts=triton320-nand:128k(sbootl),256k(u-boot),128k(env),2m(linux_kernel),83456k(userfs),32m(wince);" 

#endif	/* __CONFIG_H */
