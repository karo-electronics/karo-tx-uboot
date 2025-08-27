// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 NXP
 */

#include <asm/io.h>
#include <malloc.h>
#include <clk-uclass.h>
#include <clk.h>
#include <dm/device.h>
#include <dm/devres.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/math64.h>

#include "clk.h"

#define UBOOT_DM_CLK_IMX_FRACN_GPPLL "imx_clk_fracn_gppll"

#define PLL_CTRL		0x0
#define HW_CTRL_SEL		BIT(16)
#define CLKMUX_BYPASS		BIT(2)
#define CLKMUX_EN		BIT(1)
#define POWERUP_MASK		BIT(0)

#define PLL_ANA_PRG		0x10
#define PLL_SPREAD_SPECTRUM	0x30

#define PLL_NUMERATOR		0x40
#define PLL_MFN_MASK		GENMASK(31, 2)

#define PLL_DENOMINATOR		0x50
#define PLL_MFD_MASK		GENMASK(29, 0)

#define PLL_DIV			0x60
#define PLL_MFI_MASK		GENMASK(24, 16)
#define PLL_RDIV_MASK		GENMASK(15, 13)
#define PLL_ODIV_MASK		GENMASK(7, 0)

#define PLL_STATUS		0xF0
#define LOCK_STATUS		BIT(0)

#define LOCK_TIMEOUT_US		200

#define MFI_MIN			1
#define MFI_MAX			((1 << 9) - 1)
#define MFN_MIN			0
#define MFN_MAX			((1 << 30) - 1)
#define MFD_MIN			1
#define MFD_MAX			((1 << 30) - 1)
#define RDIV_MIN		1
#define RDIV_MAX		7
#define ODIV_MIN		2
#define ODIV_MAX		255
#define FVCO_MIN		24000000UL
#define FVCO_MAX		594000000UL

#define PLL_FRACN_GP(_rate, _mfi, _mfn, _mfd, _rdiv, _odiv)	\
	{							\
		.rate	=	_rate,				\
		.mfi	=	_mfi,				\
		.mfn	=	_mfn,				\
		.mfd	=	_mfd,				\
		.rdiv	=	_rdiv,				\
		.odiv	=	_odiv,				\
	}							\

#define PLL_FRACN_GP_INTEGER(_rate, _mfi, _rdiv, _odiv)	\
	{						\
		.rate	=	_rate,			\
		.mfi	=	_mfi,			\
		.mfn	=	0,			\
		.mfd	=	0,			\
		.rdiv	=	_rdiv,			\
		.odiv	=	_odiv,			\
	}

struct clk_fracn_gppll {
	struct clk clk;
	void __iomem *base;
	const struct imx_fracn_gppll_rate_table *rate_table;
	int rate_count;
	u32 flags;
};

/*
 * Fvco = (Fref / rdiv) * (MFI + MFN / MFD)
 * Fout = Fvco / odiv
 * The (Fref / rdiv) should be in range 20MHz to 40MHz
 * The Fvco should be in range 2.5Ghz to 5Ghz
 */
static const struct imx_fracn_gppll_rate_table fracn_tbl[] = {
	PLL_FRACN_GP(650000000U, 162, 50, 100, 0, 6),
	PLL_FRACN_GP(600000000U, 200, 0, 1, 0, 8),
	PLL_FRACN_GP(594000000U, 198, 0, 1, 0, 8),
	PLL_FRACN_GP(560000000U, 140, 0, 1, 0, 6),
	PLL_FRACN_GP(498000000U, 166, 0, 1, 0, 8),
	PLL_FRACN_GP(484000000U, 121, 0, 1, 0, 6),
	PLL_FRACN_GP(445333333U, 167, 0, 1, 0, 9),
	PLL_FRACN_GP(400000000U, 200, 0, 1, 0, 12),
	PLL_FRACN_GP(393216000U, 163, 84, 100, 0, 10),
	PLL_FRACN_GP(300000000U, 150, 0, 1, 0, 12),
	PLL_FRACN_GP(200000000U, 200, 0, 1, 0, 24)
};

struct imx_fracn_gppll_clk imx_fracn_gppll = {
	.rate_table = fracn_tbl,
	.rate_count = ARRAY_SIZE(fracn_tbl),
};

/*
 * Fvco = (Fref / rdiv) * MFI
 * Fout = Fvco / odiv
 * The (Fref / rdiv) should be in range 20MHz to 40MHz
 * The Fvco should be in range 2.5Ghz to 5Ghz
 */
static const struct imx_fracn_gppll_rate_table int_tbl[] = {
	PLL_FRACN_GP_INTEGER(1700000000U, 141, 1, 2),
	PLL_FRACN_GP_INTEGER(1400000000U, 175, 1, 3),
	PLL_FRACN_GP_INTEGER(900000000U, 150, 1, 4),
	PLL_FRACN_GP_INTEGER(800000000U, 200, 1, 6),
};

struct imx_fracn_gppll_clk imx_fracn_gppll_integer = {
	.rate_table = int_tbl,
	.rate_count = ARRAY_SIZE(int_tbl),
};

struct imx_fracn_pll_params {
	unsigned long mfi;
	unsigned long mfn;
	unsigned long mfd;
	unsigned long rdiv;
	unsigned long odiv;
	u64 fref;
	u64 fvco;
};

static inline struct clk_fracn_gppll *to_clk_fracn_gppll(struct clk *clk)
{
	return container_of(clk, struct clk_fracn_gppll, clk);
}

static const struct imx_fracn_gppll_rate_table *
imx_get_pll_settings(struct clk_fracn_gppll *pll, unsigned long rate)
{
	const struct imx_fracn_gppll_rate_table *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++)
		if (rate == rate_table[i].rate)
			return &rate_table[i];

	return NULL;
}

static inline unsigned long imx_fracn_calc_pll_rate(struct imx_fracn_pll_params *p)
{
	u64 fvco = p->fref * (p->mfi * p->mfd + p->mfn);
	unsigned long rate;

	if (WARN_ON(!p->mfd))
		return 0;
	if (WARN_ON(!p->rdiv))
		return 0;
	if (WARN_ON(!p->odiv))
		return 0;
	p->fvco = div64_ul(fvco, p->mfd);
	rate = div64_ul(p->fvco, p->odiv * p->rdiv);
	return rate;
}

static int imx_fracn_calc_pll_settings(struct clk_fracn_gppll *pll,
				       unsigned long parent_rate, unsigned long rate,
				       struct imx_fracn_pll_params *pll_params)
{
	unsigned long minerr = ULONG_MAX;
	struct imx_fracn_pll_params p = {};
	u64 fvco;
	unsigned int best_mfi;
	unsigned int best_rdiv;
	unsigned int best_odiv = 0;

	if (rate > FVCO_MAX || rate < parent_rate)
		return -ERANGE;

	p.fref = parent_rate;
	p.mfd = parent_rate;
	p.odiv = ODIV_MIN - 1;
	p.mfi = 0;
	for (p.rdiv = RDIV_MIN; p.rdiv <= RDIV_MAX; p.rdiv++) {
		unsigned long err = ULONG_MAX;

		for (p.odiv = ODIV_MIN; p.odiv <= ODIV_MAX; p.odiv++) {
			u64 freq;

			p.mfi = div64_ul((u64)rate * p.rdiv * p.odiv, parent_rate);
			if (p.mfi > MFI_MAX)
				break;

			freq = div64_ul(p.fref * p.mfi, p.rdiv * p.odiv);
			if (WARN_ON(freq > rate))
				continue;
			err = rate - freq;
			if (err < minerr) {
				minerr = err;
				best_rdiv = p.rdiv;
				best_odiv = p.odiv;
				best_mfi = p.mfi;
			}
			if (err == 0)
				break;
		}
		if (err == 0)
			break;
	}
	if (WARN_ON(!best_odiv))
		return -ERANGE;

	p.odiv = best_odiv;
	p.rdiv = best_rdiv;
	p.mfi = best_mfi;

	fvco = (u64)rate * p.rdiv * p.odiv;
	/*
	 * MFN = Fvco / Fref - MFI * MFD <=> MFN = Fvco - MFI * MFD * Fref
	 * MFD == Fref => MFN = Fvco - MFI * Fref
	 */
	p.mfn = fvco - p.mfi * p.fref;
	if (WARN_ON((long)p.mfn < 0))
		p.mfn = 0;
	*pll_params = p;
	return 0;
}

static unsigned long clk_fracn_gppll_round_rate(struct clk *clk, unsigned long rate)
{
	struct clk_fracn_gppll *pll = to_clk_fracn_gppll(clk);
	const struct imx_fracn_gppll_rate_table *rate_table;
	struct imx_fracn_pll_params pll_params;
	unsigned long prate = clk_get_parent_rate(clk);

	rate_table = imx_get_pll_settings(pll, rate);
	if (rate_table)
		return rate_table->rate;

	if (imx_fracn_calc_pll_settings(pll, prate, rate, &pll_params))
		return 0;

	return imx_fracn_calc_pll_rate(&pll_params);
}

static unsigned long clk_fracn_gppll_recalc_rate(struct clk *clk)
{
	struct clk_fracn_gppll *pll = to_clk_fracn_gppll(clk);
	const struct imx_fracn_gppll_rate_table *rate_table = pll->rate_table;
	u32 pll_numerator, pll_denominator, pll_div;
	u32 mfi, mfn;
	unsigned long mfd, rdiv, odiv;
	u64 fvco = clk_get_parent_rate(clk);
	unsigned long rate = 0;
	int i;

	pll_numerator = readl_relaxed(pll->base + PLL_NUMERATOR);
	mfn = FIELD_GET(PLL_MFN_MASK, pll_numerator);

	pll_denominator = readl_relaxed(pll->base + PLL_DENOMINATOR);
	mfd = FIELD_GET(PLL_MFD_MASK, pll_denominator);

	pll_div = readl_relaxed(pll->base + PLL_DIV);
	mfi = FIELD_GET(PLL_MFI_MASK, pll_div);

	rdiv = FIELD_GET(PLL_RDIV_MASK, pll_div);
	odiv = FIELD_GET(PLL_ODIV_MASK, pll_div);

	/*
	 * Sometimes, the recalculated rate has deviation due to
	 * the frac part. So find the accurate pll rate from the table
	 * first, if no match rate in the table, use the rate calculated
	 * from the equation below.
	 */
	for (i = 0; i < pll->rate_count; i++) {
		if (rate_table[i].mfn == mfn && rate_table[i].mfi == mfi &&
		    rate_table[i].mfd == mfd && rate_table[i].rdiv == rdiv &&
		    rate_table[i].odiv == odiv)
			rate = rate_table[i].rate;
	}

	if (rate)
		return rate;

	if (!mfd)
		mfd = 1;

	if (!rdiv)
		rdiv = rdiv + 1;

	switch (odiv) {
	case 0:
		odiv = 2;
		break;
	case 1:
		odiv = 3;
		break;
	default:
		break;
	}

	if (pll->flags & CLK_FRACN_GPPLL_INTEGER) {
		/* Fvco = (Fref / rdiv) * MFI */
		fvco = fvco * mfi;
		do_div(fvco, rdiv * odiv);
	} else {
		/* Fvco = (Fref / rdiv) * (MFI + MFN / MFD) */
		fvco = fvco * mfi * mfd + fvco * mfn;
		do_div(fvco, mfd * rdiv * odiv);
	}

	return (unsigned long)fvco;
}

static int clk_fracn_gppll_wait_lock(struct clk_fracn_gppll *pll)
{
	u32 val;

	return readl_poll_timeout(pll->base + PLL_STATUS, val,
				  val & LOCK_STATUS, LOCK_TIMEOUT_US);
}

static ulong clk_fracn_gppll_set_rate(struct clk *clk, unsigned long drate)
{
	struct clk_fracn_gppll *pll = to_clk_fracn_gppll(clk);
	const struct imx_fracn_gppll_rate_table *rate_table;
	struct imx_fracn_pll_params pp;
	u32 tmp, pll_div, ana_mfn;
	int ret;

	debug("Setting clk %s rate to %lu.%03lu MHz\n", clk->dev ? clk->dev->name : "<NULL>",
	      drate / 1000000, drate / 1000 % 1000);
	rate_table = imx_get_pll_settings(pll, drate);

	if (rate_table) {
		pp.mfi = rate_table->mfi;
		pp.mfn = rate_table->mfn;
		pp.mfd = rate_table->mfd;
		pp.rdiv = rate_table->rdiv;
		pp.odiv = rate_table->odiv;
		debug("Using rate_table[%lu]: mfi=%lu mfn=%lu mfd=%lu rdiv=%lu odiv=%lu\n",
		      rate_table - pll->rate_table, pp.mfi,
		      pp.mfn, pp.mfd, pp.rdiv, pp.odiv);

	} else {
		unsigned long prate = clk_get_parent_rate(clk);

		ret = imx_fracn_calc_pll_settings(pll, prate, drate, &pp);
		if (ret)
			return ret;
	}

	/* Hardware control select disable. PLL is control by register */
	tmp = readl_relaxed(pll->base + PLL_CTRL);
	tmp &= ~HW_CTRL_SEL;
	writel_relaxed(tmp, pll->base + PLL_CTRL);

	/* Disable output */
	tmp = readl_relaxed(pll->base + PLL_CTRL);
	tmp &= ~CLKMUX_EN;
	writel_relaxed(tmp, pll->base + PLL_CTRL);

	/* Power Down */
	tmp &= ~POWERUP_MASK;
	writel_relaxed(tmp, pll->base + PLL_CTRL);

	/* Disable BYPASS */
	tmp &= ~CLKMUX_BYPASS;
	writel_relaxed(tmp, pll->base + PLL_CTRL);

	pll_div = FIELD_PREP(PLL_RDIV_MASK, pp.rdiv) | pp.odiv |
		FIELD_PREP(PLL_MFI_MASK, pp.mfi);
	writel_relaxed(pll_div, pll->base + PLL_DIV);
	if (pll->flags & CLK_FRACN_GPPLL_FRACN) {
		writel_relaxed(pp.mfd, pll->base + PLL_DENOMINATOR);
		writel_relaxed(FIELD_PREP(PLL_MFN_MASK, pp.mfn), pll->base + PLL_NUMERATOR);
	}

	/* Wait for 5us according to fracn mode pll doc */
	udelay(5);

	/* Enable Powerup */
	tmp |= POWERUP_MASK;
	writel_relaxed(tmp, pll->base + PLL_CTRL);

	/* Wait Lock */
	ret = clk_fracn_gppll_wait_lock(pll);
	if (ret)
		return ret;

	/* Enable output */
	tmp |= CLKMUX_EN;
	writel_relaxed(tmp, pll->base + PLL_CTRL);

	ana_mfn = readl_relaxed(pll->base + PLL_STATUS);
	ana_mfn = FIELD_GET(PLL_MFN_MASK, ana_mfn);

	WARN(ana_mfn != pp.mfn, "ana_mfn != pp.mfn\n");

	return 0;
}

static int clk_fracn_gppll_prepare(struct clk *clk)
{
	struct clk_fracn_gppll *pll = to_clk_fracn_gppll(clk);
	u32 val;
	int ret;

	val = readl_relaxed(pll->base + PLL_CTRL);
	if (val & POWERUP_MASK)
		return 0;

	val |= CLKMUX_BYPASS;
	writel_relaxed(val, pll->base + PLL_CTRL);

	val |= POWERUP_MASK;
	writel_relaxed(val, pll->base + PLL_CTRL);

	val |= CLKMUX_EN;
	writel_relaxed(val, pll->base + PLL_CTRL);

	ret = clk_fracn_gppll_wait_lock(pll);
	if (ret)
		return ret;

	val &= ~CLKMUX_BYPASS;
	writel_relaxed(val, pll->base + PLL_CTRL);

	return 0;
}

static int clk_fracn_gppll_unprepare(struct clk *clk)
{
	struct clk_fracn_gppll *pll = to_clk_fracn_gppll(dev_get_clk_ptr(clk->dev));
	u32 val;

	val = readl_relaxed(pll->base + PLL_CTRL);
	val &= ~POWERUP_MASK;
	writel_relaxed(val, pll->base + PLL_CTRL);

	return 0;
}

static const struct clk_ops clk_fracn_gppll_ops = {
	.enable		= clk_fracn_gppll_prepare,
	.disable	= clk_fracn_gppll_unprepare,
	.get_rate	= clk_fracn_gppll_recalc_rate,
	.set_rate	= clk_fracn_gppll_set_rate,
	.round_rate	= clk_fracn_gppll_round_rate,
};

static struct clk *_imx_clk_fracn_gppll(const char *name, const char *parent_name,
					void __iomem *base,
					const struct imx_fracn_gppll_clk *pll_clk,
					u32 pll_flags)
{
	struct clk_fracn_gppll *pll;
	struct clk *clk;
	int ret;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->base = base;
	pll->rate_table = pll_clk->rate_table;
	pll->rate_count = pll_clk->rate_count;
	pll->flags = pll_flags;

	clk = &pll->clk;

	ret = clk_register(clk, UBOOT_DM_CLK_IMX_FRACN_GPPLL,
			   name, parent_name);
	if (ret) {
		pr_err("%s: failed to register pll %s %d\n", __func__, name, ret);
		kfree(pll);
		return ERR_PTR(ret);
	}

	return clk;
}

struct clk *imx_clk_fracn_gppll(const char *name, const char *parent_name, void __iomem *base,
				const struct imx_fracn_gppll_clk *pll_clk)
{
	return _imx_clk_fracn_gppll(name, parent_name, base, pll_clk, CLK_FRACN_GPPLL_FRACN);
}

struct clk *imx_clk_fracn_gppll_integer(const char *name, const char *parent_name,
					void __iomem *base,
					const struct imx_fracn_gppll_clk *pll_clk)
{
	return _imx_clk_fracn_gppll(name, parent_name, base, pll_clk, CLK_FRACN_GPPLL_INTEGER);
}

U_BOOT_DRIVER(clk_fracn_gppll) = {
	.name	= UBOOT_DM_CLK_IMX_FRACN_GPPLL,
	.id	= UCLASS_CLK,
	.ops	= &clk_fracn_gppll_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
