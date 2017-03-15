/*
 * ARM64 header for proper chain-loading with Little Kernel.
 *
 * (C) 2017 Lothar Waßmann <LW@KARO-electronics.de>
 *   based on board/qualcomm/dragonboard410c/head.S
 *     (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * Little Kernel shipped with Dragonboard410C boots standard Linux images for
 * ARM64. This file adds header that is required to boot U-Boot properly.
 *
 * For details see:
 * https://www.kernel.org/doc/Documentation/arm64/booting.txt
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>

.global _arm64_header
_arm64_header:
	b _start
	.word 0
	/* Image load offset from start of RAM, little-endian */
	.quad   CONFIG_SYS_TEXT_BASE - PHYS_SDRAM_1
	/* Effective size of kernel image, little-endian */
	.quad   0 /* 0x60000 - ignored */
	/* Informative flags, little-endian */
	.quad	0
	.quad	0				/* reserved */
	.quad	0				/* reserved */
	.quad	0				/* reserved */
	.byte	'A','R','M','d'			/* Magic number, "ARM\x64" */
	.word	0				/* reserved */
