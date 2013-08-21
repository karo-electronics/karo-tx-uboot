/*
 * Based on the iomux-v3.c from Linux kernel:
 * Copyright (C) 2008 by Sascha Hauer <kernel@pengutronix.de>
 * Copyright (C) 2009 by Jan Weitzel Phytec Messtechnik GmbH,
 *                       <armlinux@phytec.de>
 *
 * Copyright (C) 2004-2011 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/imx-common/iomux-v3.h>

static void *base = (void *)IOMUXC_BASE_ADDR;

/*
 * configures a single pad in the iomuxer
 */
void imx_iomux_v3_setup_pad(iomux_v3_cfg_t pad)
{
	u32 mux_ctrl_ofs = (pad & MUX_CTRL_OFS_MASK) >> MUX_CTRL_OFS_SHIFT;
	u32 mux_mode = (pad & MUX_MODE_MASK) >> MUX_MODE_SHIFT;
	u32 sel_input_ofs =
		(pad & MUX_SEL_INPUT_OFS_MASK) >> MUX_SEL_INPUT_OFS_SHIFT;
	u32 sel_input =
		(pad & MUX_SEL_INPUT_MASK) >> MUX_SEL_INPUT_SHIFT;
	u32 pad_ctrl_ofs =
		(pad & MUX_PAD_CTRL_OFS_MASK) >> MUX_PAD_CTRL_OFS_SHIFT;
	u32 pad_ctrl = (pad & MUX_PAD_CTRL_MASK) >> MUX_PAD_CTRL_SHIFT;

	if (mux_ctrl_ofs)
		__raw_writel(mux_mode, base + mux_ctrl_ofs);

	if (sel_input_ofs)
		__raw_writel(sel_input, base + sel_input_ofs);

#ifdef CONFIG_IOMUX_SHARE_CONF_REG
	if (pad & PAD_CTRL_VALID)
		__raw_writel((mux_mode << PAD_MUX_MODE_SHIFT) | pad_ctrl,
			base + pad_ctrl_ofs);
#else
	if ((pad & PAD_CTRL_VALID) && pad_ctrl_ofs)
		__raw_writel(pad_ctrl, base + pad_ctrl_ofs);
#endif
}

void imx_iomux_v3_setup_multiple_pads(iomux_v3_cfg_t const *pad_list,
				      unsigned count)
{
	iomux_v3_cfg_t const *p = pad_list;
	int i;

	for (i = 0; i < count; i++) {
#ifdef DEBUG
		u32 mux_ctrl_ofs = (*p & MUX_CTRL_OFS_MASK) >> MUX_CTRL_OFS_SHIFT;
		u32 mux_mode = (*p & MUX_MODE_MASK) >> MUX_MODE_SHIFT;
		u32 sel_input_ofs =
			(*p & MUX_SEL_INPUT_OFS_MASK) >> MUX_SEL_INPUT_OFS_SHIFT;
		u32 sel_input =
			(*p & MUX_SEL_INPUT_MASK) >> MUX_SEL_INPUT_SHIFT;
		u32 pad_ctrl_ofs =
			(*p & MUX_PAD_CTRL_OFS_MASK) >> MUX_PAD_CTRL_OFS_SHIFT;
		u32 pad_ctrl = (*p & MUX_PAD_CTRL_MASK) >> MUX_PAD_CTRL_SHIFT;

		printf("PAD[%2d]=%016llx mux[%03x]=%02x pad[%03x]=%05x%c inp[%03x]=%d\n",
			i, *p, mux_ctrl_ofs, mux_mode, pad_ctrl_ofs, pad_ctrl,
			*p & PAD_CTRL_VALID ? ' ' : '!', sel_input_ofs, sel_input);
#endif
		imx_iomux_v3_setup_pad(*p++);
	}
}
