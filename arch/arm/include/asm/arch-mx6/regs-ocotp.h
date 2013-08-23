/*
 * Freescale i.MX6 OCOTP Register Definitions
 *
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
 * based on:
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
 *
 * Based on code from LTIB:
 * Copyright 2008-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __MX6_REGS_OCOTP_H__
#define __MX6_REGS_OCOTP_H__

#ifndef	__ASSEMBLY__
#define mx6_ocotp_reg_32(r)	mxs_reg_32(hw_ocotp_##r)
#define ocotp_reg_32(r)		reg_32(hw_ocotp_##r)

struct mx6_ocotp_regs {
	mx6_ocotp_reg_32(ctrl);			/* 0x000 */
	ocotp_reg_32(timing);			/* 0x010 */
	ocotp_reg_32(data);			/* 0x020 */
	ocotp_reg_32(read_ctrl);		/* 0x030 */
	ocotp_reg_32(read_fuse_data);		/* 0x040 */
	ocotp_reg_32(sw_sticky);		/* 0x050 */
	mx6_ocotp_reg_32(scs);			/* 0x060 */
	reg_32(rsrvd1);				/* 0x070 */
	reg_32(rsrvd2);				/* 0x080 */
	ocotp_reg_32(version);			/* 0x090 */

	reg_32(rsrvd3[54]);			/* 0x0a0 - 0x3ff */

	/* bank 0 */
	ocotp_reg_32(lock);			/* 0x400 */
	ocotp_reg_32(cfg0);			/* 0x410 */
	ocotp_reg_32(cfg1);			/* 0x420 */
	ocotp_reg_32(cfg2);			/* 0x430 */
	ocotp_reg_32(cfg3);			/* 0x440 */
	ocotp_reg_32(cfg4);			/* 0x450 */
	ocotp_reg_32(cfg5);			/* 0x460 */
	ocotp_reg_32(cfg6);			/* 0x470 */

	/* bank 1 */
	ocotp_reg_32(mem0);			/* 0x480 */
	ocotp_reg_32(mem1);			/* 0x490 */
	ocotp_reg_32(mem2);			/* 0x4a0 */
	ocotp_reg_32(mem3);			/* 0x4b0 */
	ocotp_reg_32(mem4);			/* 0x4c0 */
	ocotp_reg_32(ana0);			/* 0x4d0 */
	ocotp_reg_32(ana1);			/* 0x4e0 */
	ocotp_reg_32(ana2);			/* 0x4f0 */

	/* bank 2 */
	reg_32(rsrvd4[8]);			/* 0x500 - 0x57f */

	/* bank 3 */
	ocotp_reg_32(srk0);			/* 0x580 */
	ocotp_reg_32(srk1);			/* 0x590 */
	ocotp_reg_32(srk2);			/* 0x5a0 */
	ocotp_reg_32(srk3);			/* 0x5b0 */
	ocotp_reg_32(srk4);			/* 0x5c0 */
	ocotp_reg_32(srk5);			/* 0x5d0 */
	ocotp_reg_32(srk6);			/* 0x5e0 */
	ocotp_reg_32(srk7);			/* 0x5f0 */

	/* bank 4 */
	ocotp_reg_32(hsjc_resp0);		/* 0x600 */
	ocotp_reg_32(hsjc_resp1);		/* 0x610 */
	ocotp_reg_32(mac0);			/* 0x620 */
	ocotp_reg_32(mac1);			/* 0x630 */
	reg_32(rsrvd5[2]);			/* 0x640 - 0x65f */
	ocotp_reg_32(gp1);			/* 0x660 */
	ocotp_reg_32(gp2);			/* 0x670 */

	/* bank 5 */
	reg_32(rsrvd6[5]);			/* 0x680 - 0x6cf */
	ocotp_reg_32(misc_conf);		/* 0x6d0 */
	ocotp_reg_32(field_return);		/* 0x6e0 */
	ocotp_reg_32(srk_revoke);		/* 0x6f0 */
};

#endif

#define OCOTP_CTRL_BUSY				(1 << 8)
#define OCOTP_CTRL_ERROR			(1 << 9)
#define OCOTP_CTRL_RELOAD_SHADOWS		(1 << 10)

#define OCOTP_RD_CTRL_READ_FUSE			(1 << 0)

#define	OCOTP_VERSION_MAJOR_MASK		(0xff << 24)
#define	OCOTP_VERSION_MAJOR_OFFSET		24
#define	OCOTP_VERSION_MINOR_MASK		(0xff << 16)
#define	OCOTP_VERSION_MINOR_OFFSET		16
#define	OCOTP_VERSION_STEP_MASK			0xffff
#define	OCOTP_VERSION_STEP_OFFSET		0

#endif /* __MX6_REGS_OCOTP_H__ */
