/*
 * (C) Copyright 2012  Lothar Waßmann <LW@KARO-electronics.de>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
	. = 0x00000000;
	.text :
	{
		__uboot_img_start = .;
		. = 0x400;
		__ivt_start = .;
		KEEP(*(.ivt*))
		. = 0x1000;
		__ivt_end = .;
		*(.__image_copy_start)
		*(.vectors)
		CPUDIR/start.o (.text*)
		*(.text*)
	} = 0xadde01f0

	. = ALIGN(4);
	.rodata : { *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*))) }

	. = ALIGN(4);
	.data : {
		*(.data*)
		. = ALIGN(4);
	}

	.u_boot_list : {
		KEEP(*(SORT(.u_boot_list*)));
	}

	. = ALIGN(4);
	.image_copy_end :
	{
		*(.__image_copy_end)
	}

	.rel_dyn_start :
	{
		*(.__rel_dyn_start)
	}

	.rel.dyn :
	{
		*(.rel*)
	}

	.rel_dyn_end :
	{
		*(.__rel_dyn_end)
	}

	.pad :
	{
		*(.pad)
		. = ALIGN(4096);
	} = 0x01f0adde

	_image_binary_end = . + (__ivt_end - __uboot_img_start);

	.uboot_img_end :
	{
		KEEP(*(.__uboot_img_end))
	}

#ifdef CONFIG_SECURE_BOOT
	. = CONFIG_SYS_TEXT_BASE + 0x71000;
	.csf_data :
	{
		*(.__csf_data)
		. = . + 0x2000;
	}
#endif
	__uboot_img_len = . - __uboot_img_start;

/*
 * Compiler-generated __bss_start and __bss_end, see arch/arm/lib/bss.c
 * __bss_base and __bss_limit are for linker only (overlay ordering)
 */

	.bss_start __rel_dyn_start (OVERLAY) : {
		KEEP(*(.__bss_start));
		__bss_base = .;
	}

	.bss __bss_base (OVERLAY) : {
		*(.bss*)
		. = ALIGN(4);
		__bss_limit = .;
	}
	.bss_end __bss_limit (OVERLAY) : {
		KEEP(*(.__bss_end));
	}

	.dynsym _image_binary_end : { *(.dynsym) }
	/DISCARD/ : { *(.debug*) }
	/DISCARD/ : { *(.note*) }
	/DISCARD/ : { *(.comment*) }
	/DISCARD/ : { *(.dynbss) }
	/DISCARD/ : { *(.dynstr*) }
	/DISCARD/ : { *(.dynamic*) }
	/DISCARD/ : { *(.plt*) }
	/DISCARD/ : { *(.interp*) }
	/DISCARD/ : { *(.gnu.hash) }
	/DISCARD/ : { *(.gnu*) }
	/DISCARD/ : { *(.ARM.exidx*) }
	/DISCARD/ : { *(.gnu.linkonce.armexidx.*) }
}
