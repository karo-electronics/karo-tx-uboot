// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 * Author: Gabriel Fernandez <gabriel.fernandez@st.com> for STMicroelectronics.
 */

#define LOG_CATEGORY UCLASS_CLK

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <log.h>
#include <asm/io.h>
#include <linux/clk-provider.h>
#include "clk-stm32-core.h"

int stm32_rcc_init(struct device *dev,
		   const struct stm32_clock_match_data *data,
		   void __iomem *base)
{
	int i;

	for (i = 0; i < data->num_clocks; i++) {
		const struct clock_config *cfg = &data->tab_clocks[i];
		struct clk *clk = ERR_PTR(-ENOENT);

		if (data->check_security) {
			if ((*data->check_security)(base, cfg))
				continue;
		}

		if (cfg->func)
			clk = (*cfg->func)(NULL, data, base, NULL, cfg);

		if (IS_ERR(clk)) {
			log_err("%s: failed to register clock %s\n", __func__,
				cfg->name);

			return  PTR_ERR(clk);
		}

		clk->id = cfg->id;
	}

	return 0;
}

static const struct clk_ops *clk_dev_ops(struct udevice *dev)
{
	return (const struct clk_ops *)dev->driver->ops;
}

static int stm32_clk_enable(struct clk *clk)
{
	const struct clk_ops *ops;
	struct clk *clkp = NULL;

	if (!clk->id || clk_get_by_id(clk->id, &clkp))
		return -ENOENT;

	ops = clk_dev_ops(clkp->dev);
	if (!ops->enable)
		return 0;

	return ops->enable(clkp);
}

static int stm32_clk_disable(struct clk *clk)
{
	const struct clk_ops *ops;
	struct clk *clkp = NULL;

	if (!clk->id || clk_get_by_id(clk->id, &clkp))
		return -ENOENT;

	ops = clk_dev_ops(clkp->dev);
	if (!ops->disable)
		return 0;

	return ops->disable(clkp);
}

static ulong stm32_clk_get_rate(struct clk *clk)
{
	const struct clk_ops *ops;
	struct clk *clkp = NULL;

	if (!clk->id || clk_get_by_id(clk->id, &clkp))
		return -ENOENT;

	ops = clk_dev_ops(clkp->dev);
	if (!ops->get_rate)
		return -ENOSYS;

	return ops->get_rate(clkp);
}

ulong stm32_clk_set_rate(struct clk *clk, unsigned long clk_rate)
{
	const struct clk_ops *ops;
	struct clk *clkp = NULL;

	if (!clk->id || clk_get_by_id(clk->id, &clkp))
		return -ENOENT;

	ops = clk_dev_ops(clkp->dev);
	if (!ops->set_rate)
		return -ENOSYS;

	return ops->set_rate(clkp, clk_rate);
}

int clk_stm32_get_by_name(const char *name, struct clk **clkp)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_CLK, &uc);
	if (ret)
		return ret;

	uclass_foreach_dev(dev, uc) {
		if (!strcmp(name, dev->name)) {
			struct clk *clk = dev_get_clk_ptr(dev);

			if (clk) {
				*clkp = clk;
				return 0;
			}
			break;
		}
	}

	return -ENOENT;
}

ulong clk_stm32_get_rate_by_name(const char *name)
{
	struct clk *clk;

	if (!clk_stm32_get_by_name(name, &clk))
		return clk_get_rate(clk);

	return 0;
}

const struct clk_ops stm32_clk_ops = {
	.enable = stm32_clk_enable,
	.disable = stm32_clk_disable,
	.get_rate = stm32_clk_get_rate,
	.set_rate = stm32_clk_set_rate,
};

#define UBOOT_DM_CLK_STM32_SETCLR_GATE "clk_stm32_setclr_gate"

#define RCC_MP_ENCLRR_OFFSET	4

static void clk_setclr_gate_endisable(struct clk *clk, int enable)
{
	struct clk_gate *gate = to_clk_gate(clk);

	if (enable)
		writel(BIT(gate->bit_idx), gate->reg);
	else
		writel(BIT(gate->bit_idx), gate->reg + RCC_MP_ENCLRR_OFFSET);
}

static int clk_setclr_gate_enable(struct clk *clk)
{
	clk_setclr_gate_endisable(clk, 1);

	return 0;
}

static int clk_setclr_gate_disable(struct clk *clk)
{
	clk_setclr_gate_endisable(clk, 0);

	return 0;
}

const struct clk_ops clk_stm32_setclr_gate_ops = {
	.enable = clk_setclr_gate_enable,
	.disable = clk_setclr_gate_disable,
	.get_rate = clk_generic_get_rate,
};

struct clk *clk_stm32_register_setclr_gate(struct device *dev,
					   const char *name,
					   const char *parent_name,
					   unsigned long flags,
					   void __iomem *reg, u8 bit_idx,
					   u8 clk_gate_flags, spinlock_t *lock)
{
	struct clk_gate *gate;
	struct clk *clk;
	int ret;

	/* allocate the gate */
	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	/* struct clk_gate assignments */
	gate->reg = reg;
	gate->bit_idx = bit_idx;
	gate->flags = clk_gate_flags;

	clk = &gate->clk;
	clk->flags = flags;

	ret = clk_register(clk, UBOOT_DM_CLK_STM32_SETCLR_GATE, name,
			   parent_name);
	if (ret) {
		kfree(gate);
		return ERR_PTR(ret);
	}

	return clk;
}

U_BOOT_DRIVER(clk_stm32_setclr_gate) = {
	.name	= UBOOT_DM_CLK_STM32_SETCLR_GATE,
	.id	= UCLASS_CLK,
	.ops	= &clk_stm32_setclr_gate_ops,
};

struct clk *clk_stm32_register_composite(const char *name,
					 const char * const *parent_names,
					 int num_parents,
					 unsigned long flags,
					 void __iomem *base,
					 const struct stm32_mux_cfg *pcfg,
					 const struct stm32_div_cfg *dcfg,
					 const struct stm32_gate_cfg *gcfg)
{
	struct clk *clk = ERR_PTR(-ENOMEM);
	struct clk_mux *mux = NULL;
	struct clk_gate *gate = NULL;
	struct clk_divider *div = NULL;
	struct clk *mux_clk = NULL;
	const struct clk_ops *mux_ops = NULL;
	struct clk *gate_clk = NULL;
	const struct clk_ops *gate_ops = NULL;
	struct clk *div_clk = NULL;
	const struct clk_ops *div_ops = NULL;

	if (pcfg) {
		mux = kzalloc(sizeof(*mux), GFP_KERNEL);
		if (!mux)
			goto fail;

		mux->reg = base + pcfg->reg_off;
		mux->shift = pcfg->shift;
		mux->mask = BIT(pcfg->width) - 1;
		mux->num_parents = pcfg->num_parents;
		mux->flags = 0;
		mux->parent_names = pcfg->parent_names;

		mux_clk = &mux->clk;
		mux_ops = &clk_mux_ops;
	}

	if (dcfg) {
		div = kzalloc(sizeof(*div), GFP_KERNEL);
		if (!div)
			goto fail;

		div->reg = base + dcfg->reg_off;
		div->shift = dcfg->shift;
		div->width = dcfg->width;
		div->width = dcfg->width;
		div->flags = dcfg->div_flags;
		div->table = dcfg->table;
	}

	if (gcfg) {
		gate = kzalloc(sizeof(*gate), GFP_KERNEL);
		if (!gate)
			goto fail;

		gate->reg = base + gcfg->reg_off;
		gate->bit_idx = gcfg->bit_idx;
		gate->flags = gcfg->gate_flags;

		gate_clk = &gate->clk;
		gate_ops = &clk_gate_ops;

		if (gcfg->set_clr)
			gate_ops = &clk_stm32_setclr_gate_ops;
	}

	clk = clk_register_composite(NULL, name,
				     parent_names, num_parents,
				     mux_clk, mux_ops,
				     div_clk, div_ops,
				     gate_clk, gate_ops,
				     flags);
	if (IS_ERR(clk))
		goto fail;

	return clk;

fail:
	kfree(gate);
	kfree(div);
	kfree(mux);
	return ERR_CAST(clk);
}

struct clk *_clk_stm32_gate_register(struct device *dev,
				     const struct stm32_clock_match_data *data,
				     void __iomem *base,
				     spinlock_t *lock,
				     const struct clock_config *cfg)
{
	struct stm32_clk_gate_cfg *clk_cfg = cfg->clock_cfg;
	const struct stm32_gate_cfg *gate_cfg = &data->gates[clk_cfg->gate_id];
	struct clk *clk;

	if (gate_cfg->set_clr) {
		clk = clk_stm32_register_setclr_gate(dev, cfg->name,
						     cfg->parent_name,
						     cfg->flags,
						     base + gate_cfg->reg_off,
						     gate_cfg->bit_idx,
						     gate_cfg->gate_flags,
						     lock);
	} else {
		clk = clk_register_gate(dev, cfg->name, cfg->parent_name,
					cfg->flags,
					base + gate_cfg->reg_off,
					gate_cfg->bit_idx,
					gate_cfg->gate_flags,
					lock);
	}
	return clk;
}

struct clk *
_clk_stm32_register_composite(struct device *dev,
			      const struct stm32_clock_match_data *data,
			      void __iomem *base, spinlock_t *lock,
			      const struct clock_config *cfg)
{
	struct stm32_clk_composite_cfg *composite = cfg->clock_cfg;
	const struct stm32_mux_cfg *mux_cfg = NULL;
	const struct stm32_gate_cfg *gate_cfg = NULL;
	const struct stm32_div_cfg *div_cfg = NULL;
	const char *const *parent_names;
	int num_parents;

	if  (composite->mux_id != NO_STM32_MUX) {
		mux_cfg = &data->muxes[composite->mux_id];
		parent_names = mux_cfg->parent_names;
		num_parents = mux_cfg->num_parents;
	} else {
		parent_names = &cfg->parent_name;
		num_parents = 1;
	}

	if  (composite->gate_id != NO_STM32_GATE)
		gate_cfg = &data->gates[composite->gate_id];

	if  (composite->div_id != NO_STM32_DIV)
		div_cfg = &data->dividers[composite->div_id];

	return clk_stm32_register_composite(cfg->name, parent_names,
					    num_parents, cfg->flags, base,
					    mux_cfg, div_cfg, gate_cfg);
}
