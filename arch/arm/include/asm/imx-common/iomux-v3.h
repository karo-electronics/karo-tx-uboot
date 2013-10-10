/*
 * Based on Linux i.MX iomux-v3.h file:
 * Copyright (C) 2009 by Jan Weitzel Phytec Messtechnik GmbH,
 *			<armlinux@phytec.de>
 *
 * Copyright (C) 2011 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ASM_ARCH_IOMUX_V3_H__
#define __ASM_ARCH_IOMUX_V3_H__

#include <common.h>

/*
 *	build IOMUX_PAD structure
 *
 * This iomux scheme is based around pads, which are the physical balls
 * on the processor.
 *
 * - Each pad has a pad control register (IOMUXC_SW_PAD_CTRL_x) which controls
 *   things like driving strength and pullup/pulldown.
 * - Each pad can have but not necessarily does have an output routing register
 *   (IOMUXC_SW_MUX_CTL_PAD_x).
 * - Each pad can have but not necessarily does have an input routing register
 *   (IOMUXC_x_SELECT_INPUT)
 *
 * The three register sets do not have a fixed offset to each other,
 * hence we order this table by pad control registers (which all pads
 * have) and put the optional i/o routing registers into additional
 * fields.
 *
 * The naming convention for the pad modes is SOC_PAD_<padname>__<padmode>
 * If <padname> or <padmode> refers to a GPIO, it is named GPIO_<unit>_<num>
 *
 * IOMUX/PAD Bit field definitions
 *
 * MUX_CTRL_OFS:		 0..11 (12)
 * PAD_CTRL_OFS:		12..23 (12)
 * SEL_INPUT_OFS:		24..35 (12)
 * MUX_MODE + SION:		36..40  (5)
 * PAD_CTRL + PAD_CTRL_VALID:	41..58 (18)
 * SEL_INP:			59..62  (4)
 * reserved:			  63    (1)
*/

typedef u64 iomux_v3_cfg_t;

#define MUX_CTRL_OFS_SHIFT	0
#define MUX_CTRL_OFS_MASK	((iomux_v3_cfg_t)0xfff << MUX_CTRL_OFS_SHIFT)
#define MUX_PAD_CTRL_OFS_SHIFT	12
#define MUX_PAD_CTRL_OFS_MASK	((iomux_v3_cfg_t)0xfff << \
	MUX_PAD_CTRL_OFS_SHIFT)
#define MUX_SEL_INPUT_OFS_SHIFT	24
#define MUX_SEL_INPUT_OFS_MASK	((iomux_v3_cfg_t)0xfff << \
	MUX_SEL_INPUT_OFS_SHIFT)

#define MUX_MODE_SHIFT		36
#define MUX_MODE_MASK		((iomux_v3_cfg_t)0x1f << MUX_MODE_SHIFT)
#define MUX_PAD_CTRL_SHIFT	41
#define MUX_PAD_CTRL_MASK	((iomux_v3_cfg_t)0x1ffff << MUX_PAD_CTRL_SHIFT)
#define MUX_SEL_INPUT_SHIFT	59
#define MUX_SEL_INPUT_MASK	((iomux_v3_cfg_t)0xf << MUX_SEL_INPUT_SHIFT)

#define __MUX_PAD_CTRL(x)	((x) | __PAD_CTRL_VALID)
#define MUX_PAD_CTRL(x)		(((iomux_v3_cfg_t)__MUX_PAD_CTRL(x) << \
						MUX_PAD_CTRL_SHIFT))

#define IOMUX_PAD(pad_ctrl_ofs, mux_ctrl_ofs, mux_mode, sel_input_ofs,	\
		sel_input, pad_ctrl)					\
	(((iomux_v3_cfg_t)(mux_ctrl_ofs) << MUX_CTRL_OFS_SHIFT)     |	\
	((iomux_v3_cfg_t)(mux_mode)      << MUX_MODE_SHIFT)         |	\
	((iomux_v3_cfg_t)(pad_ctrl_ofs)  << MUX_PAD_CTRL_OFS_SHIFT) |	\
	((iomux_v3_cfg_t)(pad_ctrl)      << MUX_PAD_CTRL_SHIFT)     |	\
	((iomux_v3_cfg_t)(sel_input_ofs) << MUX_SEL_INPUT_OFS_SHIFT)|	\
	((iomux_v3_cfg_t)(sel_input)     << MUX_SEL_INPUT_SHIFT))

#define NEW_PAD_CTRL(cfg, pad)	(((cfg) & ~MUX_PAD_CTRL_MASK) | \
					MUX_PAD_CTRL(pad))

#define __NA_			0x000
#define NO_MUX_I		0
#define NO_PAD_I		0

#define NO_MUX_I		0
#define NO_PAD_I		0

#define NO_PAD_CTRL		0
#define __PAD_CTRL_VALID	(1 << 17)
#define PAD_CTRL_VALID		((iomux_v3_cfg_t)__PAD_CTRL_VALID << MUX_PAD_CTRL_SHIFT)

#ifdef CONFIG_MX6

#define PAD_CTL_HYS		__MUX_PAD_CTRL(1 << 16)

#define PAD_CTL_PUS_100K_DOWN	__MUX_PAD_CTRL(0 << 14 | PAD_CTL_PUE)
#define PAD_CTL_PUS_47K_UP	__MUX_PAD_CTRL(1 << 14 | PAD_CTL_PUE)
#define PAD_CTL_PUS_100K_UP	__MUX_PAD_CTRL(2 << 14 | PAD_CTL_PUE)
#define PAD_CTL_PUS_22K_UP	__MUX_PAD_CTRL(3 << 14 | PAD_CTL_PUE)
#define PAD_CTL_PUE		__MUX_PAD_CTRL(1 << 13 | PAD_CTL_PKE)
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

#elif defined(CONFIG_VF610)

#define PAD_MUX_MODE_SHIFT	20

#define PAD_CTL_SPEED_MED	__MUX_PAD_CTRL(1 << 12)
#define PAD_CTL_SPEED_HIGH	__MUX_PAD_CTRL(3 << 12)

#define PAD_CTL_DSE_50ohm	__MUX_PAD_CTRL(3 << 6)
#define PAD_CTL_DSE_25ohm	__MUX_PAD_CTRL(6 << 6)
#define PAD_CTL_DSE_20ohm	__MUX_PAD_CTRL(7 << 6)

#define PAD_CTL_PUS_47K_UP	__MUX_PAD_CTRL(1 << 4 | PAD_CTL_PUE)
#define PAD_CTL_PUS_100K_UP	__MUX_PAD_CTRL(2 << 4 | PAD_CTL_PUE)
#define PAD_CTL_PKE		__MUX_PAD_CTRL(1 << 3)
#define PAD_CTL_PUE		__MUX_PAD_CTRL(1 << 2 | PAD_CTL_PKE)

#define PAD_CTL_OBE_IBE_ENABLE	__MUX_PAD_CTRL(3 << 0)

#else

#define PAD_CTL_DVS		__MUX_PAD_CTRL(1 << 13)
#define PAD_CTL_INPUT_DDR	__MUX_PAD_CTRL(1 << 9)
#define PAD_CTL_HYS		__MUX_PAD_CTRL(1 << 8)

#define PAD_CTL_PKE		__MUX_PAD_CTRL(1 << 7)
#define PAD_CTL_PUE		__MUX_PAD_CTRL(1 << 6 | PAD_CTL_PKE)
#define PAD_CTL_PUS_100K_DOWN	__MUX_PAD_CTRL(0 << 4 | PAD_CTL_PUE)
#define PAD_CTL_PUS_47K_UP	__MUX_PAD_CTRL(1 << 4 | PAD_CTL_PUE)
#define PAD_CTL_PUS_100K_UP	__MUX_PAD_CTRL(2 << 4 | PAD_CTL_PUE)
#define PAD_CTL_PUS_22K_UP	__MUX_PAD_CTRL(3 << 4 | PAD_CTL_PUE)

#define PAD_CTL_ODE		__MUX_PAD_CTRL(1 << 3)

#define PAD_CTL_DSE_LOW		__MUX_PAD_CTRL(0 << 1)
#define PAD_CTL_DSE_MED		__MUX_PAD_CTRL(1 << 1)
#define PAD_CTL_DSE_HIGH	__MUX_PAD_CTRL(2 << 1)
#define PAD_CTL_DSE_MAX		__MUX_PAD_CTRL(3 << 1)

#endif

#define PAD_CTL_SRE_SLOW	__MUX_PAD_CTRL(0 << 0)
#define PAD_CTL_SRE_FAST	__MUX_PAD_CTRL(1 << 0)

#define IOMUX_CONFIG_SION	0x10

#define GPIO_PIN_MASK		0x1f
#define GPIO_PORT_SHIFT		5
#define GPIO_PORT_MASK		(0x7 << GPIO_PORT_SHIFT)
#define GPIO_PORTA		(0 << GPIO_PORT_SHIFT)
#define GPIO_PORTB		(1 << GPIO_PORT_SHIFT)
#define GPIO_PORTC		(2 << GPIO_PORT_SHIFT)
#define GPIO_PORTD		(3 << GPIO_PORT_SHIFT)
#define GPIO_PORTE		(4 << GPIO_PORT_SHIFT)
#define GPIO_PORTF		(5 << GPIO_PORT_SHIFT)

void imx_iomux_v3_setup_pad(iomux_v3_cfg_t pad);
void imx_iomux_v3_setup_multiple_pads(iomux_v3_cfg_t const *pad_list,
				     unsigned count);

#endif	/* __ASM_ARCH_IOMUX_V3_H__*/
