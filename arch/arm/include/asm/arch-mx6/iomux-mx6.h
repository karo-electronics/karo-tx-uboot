/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Auto Generate file, please don't edit it
 *
 */

#ifndef __ASM_ARCH_IOMUX_MX6_H__
#define __ASM_ARCH_IOMUX_MX6_H__

#include <asm/imx-common/iomux-v3.h>
#ifdef CONFIG_MX6Q
#include "iomux-mx6q.h"
#elif defined(CONFIG_MX6DL)
#include "iomux-mx6dl.h"
#elif defined(CONFIG_MX6SL)
#include "iomux-mx6sl.h"
#else
#error Unsupported i.MX6 variant
#endif

/*
 * Use to set PAD control
 */
#define PAD_CTL_HYS		__MUX_PAD_CTRL((1 << 16))

#define PAD_CTL_PUS_100K_DOWN	__MUX_PAD_CTRL(PAD_CTL_PULL | (0 << 14))
#define PAD_CTL_PUS_47K_UP	__MUX_PAD_CTRL(PAD_CTL_PULL | (1 << 14))
#define PAD_CTL_PUS_100K_UP	__MUX_PAD_CTRL(PAD_CTL_PULL | (2 << 14))
#define PAD_CTL_PUS_22K_UP	__MUX_PAD_CTRL(PAD_CTL_PULL | (3 << 14))

#define PAD_CTL_PULL		__MUX_PAD_CTRL(PAD_CTL_PKE | PAD_CTL_PUE)
#define PAD_CTL_PUE		__MUX_PAD_CTRL(1 << 13)
#define PAD_CTL_PKE		__MUX_PAD_CTRL(1 << 12)
#define PAD_CTL_ODE		__MUX_PAD_CTRL(1 << 11)

#define PAD_CTL_SPEED_LOW	__MUX_PAD_CTRL(1 << 6)
#define PAD_CTL_SPEED_MED	__MUX_PAD_CTRL(2 << 6)
#define PAD_CTL_SPEED_HIGH	__MUX_PAD_CTRL(3 << 6)

#define PAD_CTL_DSE_DISABLE	__MUX_PAD_CTRL(0 << 3)
#define PAD_CTL_DSE_240ohm	__MUX_PAD_CTRL(1 << 3)
#define PAD_CTL_DSE_120ohm	__MUX_PAD_CTRL(2 << 3)
#define PAD_CTL_DSE_80ohm	__MUX_PAD_CTRL(3 << 3)
#define PAD_CTL_DSE_60ohm	__MUX_PAD_CTRL(4 << 3)
#define PAD_CTL_DSE_48ohm	__MUX_PAD_CTRL(5 << 3)
#define PAD_CTL_DSE_40ohm	__MUX_PAD_CTRL(6 << 3)
#define PAD_CTL_DSE_34ohm	__MUX_PAD_CTRL(7 << 3)

#define PAD_CTL_SRE_FAST	__MUX_PAD_CTRL(1 << 0)
#define PAD_CTL_SRE_SLOW	__MUX_PAD_CTRL(0 << 0)

#define MX6_UART_PAD_CTRL	(PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | \
				PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST | PAD_CTL_HYS)

#define MX6_ECSPI_PAD_CTRL	(PAD_CTL_SRE_FAST | PAD_CTL_PUS_100K_DOWN | PAD_CTL_SPEED_MED | \
				PAD_CTL_DSE_40ohm | PAD_CTL_HYS)

#define MX6_USDHC_PAD_CTRL	(PAD_CTL_PUS_47K_UP | PAD_CTL_SPEED_MED | \
				PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST | PAD_CTL_HYS)

#define MX6_ENET_PAD_CTRL	(PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | \
				PAD_CTL_DSE_40ohm | PAD_CTL_HYS)

#define MX6_I2C_PAD_CTRL	(PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | \
				PAD_CTL_DSE_40ohm | PAD_CTL_HYS | \
				PAD_CTL_ODE | PAD_CTL_SRE_FAST)

#define MX6_PWM_PAD_CTRL	(PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | \
				PAD_CTL_DSE_40ohm | PAD_CTL_HYS | \
				PAD_CTL_SRE_FAST)

#define MX6_HIGH_DRV		PAD_CTL_DSE_120ohm

#define MX6_DISP_PAD_CTRL	MX6_HIGH_DRV

#define MX6_GPMI_PAD_CTRL0	(PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)
#define MX6_GPMI_PAD_CTRL1	(PAD_CTL_DSE_40ohm | PAD_CTL_SPEED_MED | PAD_CTL_SRE_FAST)
#define MX6_GPMI_PAD_CTRL2	(MX6_GPMI_PAD_CTRL0 | MX6_GPMI_PAD_CTRL1)

#endif /* __ASM_ARCH_IOMUX_MX6_H__ */
