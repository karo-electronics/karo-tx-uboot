// SPDX-License-Identifier: GPL-2.0+
/*
 * Renesas RZ/G2L CLK driver
 *
 * Copyright (C) 2020 Renesas Electronics Corp.
 * 
 * Based on the following driver from Linux kernel:
 * r8a774a1 Clock Pulse Generator / Module Standby and Software Reset 
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <wait_bit.h>
#include <asm/io.h>
#include <linux/bitops.h>

#include <dt-bindings/clock/renesas-cpg-mssr.h>
#include <dt-bindings/clock/r9a07g044l-cpg.h>
#include "renesas-rzg2l-cpg.h"

static int mod_clk_get(struct clk *clk, struct cpg_mssr_info *info,
			const struct mssr_mod_clk **mssr)
{
	const unsigned long clkid = clk->id & 0xffff;
	int i;

	for (i = 0; i < info->mod_clk_size; i++) {
		if (info->mod_clk[i].id !=
		    (info->mod_clk_base + MOD_CLK_PACK(clkid)))
			continue;

		*mssr = &info->mod_clk[i];
		return 0;
	}

	return -ENODEV;
}

static int rzg2l_clk_enable(struct clk *clk)
{
	struct rzg2l_clk_priv *priv = dev_get_priv(clk->dev);
	const struct mssr_mod_clk *clock;
	uint32_t value=0;
	int ret=0;
	
	if (!is_mod_clk(clk)) {
		debug("CLK: not a module clock \n");
		return -1;
	}
	
	/* Set write_mask bit, set clk enable, then check monitor status */
	mod_clk_get(clk, priv->info, &clock);
	value = (MSSR_ON(clock->bit) << 16) | MSSR_ON(clock->bit);
	setbits_le32(priv->base + CLK_ON_R(MSSR_OFF(clock->bit) * 4), value);
	ret = wait_for_bit_le32(priv->base + CLK_MON_R(MSSR_OFF(clock->bit) * 4),
					MSSR_ON(clock->bit), 1, 100, 0);
	
	return ret;
}

static int rzg2l_clk_disable(struct clk *clk)
{
	struct rzg2l_clk_priv *priv = dev_get_priv(clk->dev);
	const struct mssr_mod_clk *clock;
	uint32_t value=0;
	int ret=0;

	if (!is_mod_clk(clk)) {
		debug("CLK: not a module clock \n");
		return -1;
	}
	
	mod_clk_get(clk, priv->info, &clock);
	value = MSSR_ON(clock->bit) << 16;
	setbits_le32(priv->base +0x500 + MSSR_OFF(clock->bit) * 4, value);
	ret = wait_for_bit_le32(priv->base + CLK_MON_R(MSSR_OFF(clock->bit) * 4),
					MSSR_ON(clock->bit), 0, 100, 0);

	return ret;
}

static ulong __maybe_unused rzg2l_clk_set_rate(struct clk *clk, ulong rate)
{
	
	return 0;
}

static ulong __maybe_unused rzg2l_clk_get_rate(struct clk *clk)
{
	return 0;
}

int rzg2l_clk_probe(struct udevice *dev)
{
	int ret;
	struct rzg2l_clk_priv *priv = dev_get_priv(dev);
	struct cpg_mssr_info *info =
		(struct cpg_mssr_info *)dev_get_driver_data(dev);
	
	priv->info = info;
	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base)
		return -EINVAL;

	ret = clk_get_by_name(dev, "xinclk", &priv->clk_extal);
	if (ret < 0)
		return ret;

	return 0;
}

static int rzg2l_clk_of_xlate(struct clk *clk, struct ofnode_phandle_args *args)
{
	if (args->args_count != 2) {
		debug("Invaild args_count: %d\n", args->args_count);
		return -EINVAL;
	}

	clk->id = (args->args[0] << 16) | args->args[1];

	return 0;
}

int rzg2l_clk_remove(struct udevice *dev)
{
	struct rzg2l_clk_priv __maybe_unused *priv = dev_get_priv(dev);

	return 0;
}

const struct clk_ops rzg2l_clk_ops = {
	.enable		= rzg2l_clk_enable,
	.disable	= rzg2l_clk_disable,
	.of_xlate	= rzg2l_clk_of_xlate,
};
