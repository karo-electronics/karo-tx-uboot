/*
 * (C) Copyright 2007
 * Ka-Ro electronics GmbH <http://www.karo-electronics.de>
 *
 * based on: board/zylonite/zylonite.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/hardware.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Miscelaneous platform dependent initialisations
 */

int board_init(void)
{
	/* memory and cpu-speed are setup before relocation */
	/* so we do _nothing_ here */

	/* arch number of KARO-Board */
	gd->bd->bi_arch_number = MACH_TYPE_KARO;

	/* adress of boot parameters */
	
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	return 0;
}

int board_late_init(void)
{
	unsigned long arsr = ARSR;
	char *dlm = " ";

	setenv("stdout", "serial");
	setenv("stderr", "serial");

	printf("Last Reset caused by:");
	if (arsr & ARSR_GPR) {
		printf("%sgpio reset", dlm);
		dlm=" | ";
	}
	if (arsr & ARSR_LPMR) {
		printf("%slow power mode wakeup", dlm);
		dlm=" | ";
	}
	if (arsr & ARSR_WDR) {
		printf("%swatchdog reset", dlm);
		dlm=" | ";
	}
	if (arsr & ARSR_HWR) {
		printf("%shardware reset", dlm);
		dlm=" | ";
	}
	printf("\n");
	return 0;
}

int dram_init(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}
