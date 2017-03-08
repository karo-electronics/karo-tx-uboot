/*
 * Qualcomm APQ8016 memory map
 *
 * (C) Copyright 2016 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/armv8/mmu.h>

static struct mm_region apq8016_mem_map[] = {
	{
		.phys = 0x0UL, /* Peripheral block */
		.virt = 0x0UL,
		.size = SZ_256M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		.phys = 0x80000000UL, /* DDR */
		.virt = 0x80000000UL,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE,
	},
	{ /* List terminator */ }
};

struct mm_region *mem_map = apq8016_mem_map;
