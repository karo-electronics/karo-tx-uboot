/*
 * (C) Copyright 2007
 * Sascha Hauer, Pengutronix
 *
 * (C) Copyright 2009 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <div64.h>
#include <asm/arch/sys_proto.h>

enum pll_clocks {
	PLL1_CLOCK = 0,
	PLL2_CLOCK,
	PLL3_CLOCK,
#ifdef CONFIG_SOC_MX53
	PLL4_CLOCK,
#endif
	PLL_CLOCKS,
};

struct mxc_pll_reg *mxc_plls[PLL_CLOCKS] = {
	[PLL1_CLOCK] = (struct mxc_pll_reg *)PLL1_BASE_ADDR,
	[PLL2_CLOCK] = (struct mxc_pll_reg *)PLL2_BASE_ADDR,
	[PLL3_CLOCK] = (struct mxc_pll_reg *)PLL3_BASE_ADDR,
#ifdef	CONFIG_SOC_MX53
	[PLL4_CLOCK] = (struct mxc_pll_reg *)PLL4_BASE_ADDR,
#endif
};

#define AHB_CLK_ROOT    133333333
#define SZ_DEC_1M       1000000
#define PLL_PD_MAX      16      /* Actual pd+1 */
#define PLL_MFI_MAX     15
#define PLL_MFI_MIN     5
#define ARM_DIV_MAX     8
#define IPG_DIV_MAX     4
#define AHB_DIV_MAX     8
#define EMI_DIV_MAX     8
#define NFC_DIV_MAX     8

#define MXC_IPG_PER_CLK	MXC_IPG_PERCLK

struct fixed_pll_mfd {
	u32 ref_clk_hz;
	u32 mfd;
};

static const struct fixed_pll_mfd fixed_mfd[] = {
	{MXC_HCLK, 24 * 16},
};

struct pll_param {
	u32 pd;
	u32 mfi;
	u32 mfn;
	u32 mfd;
};

#define PLL_FREQ_MAX(ref_clk)  (4 * (ref_clk) * PLL_MFI_MAX)
#define PLL_FREQ_MIN(ref_clk) \
	((4 * (ref_clk) * PLL_MFI_MIN) / PLL_PD_MAX)
#define MAX_DDR_CLK     420000000
#define NFC_CLK_MAX     34000000

struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)MXC_CCM_BASE;

int clk_enable(struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return 0;

	if (clk->usecount++ == 0) {
		if (!clk->enable)
			return 0;
		ret = clk->enable(clk);
		if (ret)
			clk->usecount--;
	}
	return ret;
}

void clk_disable(struct clk *clk)
{
	if (!clk)
		return;

	if (!(--clk->usecount)) {
		if (clk->disable)
			clk->disable(clk);
	}
	if (clk->usecount < 0) {
		printf("%s: clk %p (%s) underflow\n", __func__, clk, clk->name);
		hang();
	}
}

int clk_get_usecount(struct clk *clk)
{
	if (clk == NULL)
		return 0;

	return clk->usecount;
}

u32 clk_get_rate(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk->rate;
}

struct clk *clk_get_parent(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk->parent;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	if (clk && clk->set_rate)
		clk->set_rate(clk, rate);
	return clk->rate;
}

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (clk == NULL || !clk->round_rate)
		return 0;

	return clk->round_rate(clk, rate);
}

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	debug("Setting parent of clk %p to %p (%p)\n", clk, parent,
		clk ? clk->parent : NULL);

	if (!clk || clk == parent)
		return 0;

	if (clk->set_parent) {
		int ret;

		ret = clk->set_parent(clk, parent);
		if (ret)
			return ret;
	}
	clk->parent = parent;
	return 0;
}

void set_usboh3_clk(void)
{
	clrsetbits_le32(&mxc_ccm->cscmr1,
			MXC_CCM_CSCMR1_USBOH3_CLK_SEL_MASK,
			MXC_CCM_CSCMR1_USBOH3_CLK_SEL(1));
	clrsetbits_le32(&mxc_ccm->cscdr1,
			MXC_CCM_CSCDR1_USBOH3_CLK_PODF_MASK |
			MXC_CCM_CSCDR1_USBOH3_CLK_PRED_MASK,
			MXC_CCM_CSCDR1_USBOH3_CLK_PRED(4) |
			MXC_CCM_CSCDR1_USBOH3_CLK_PODF(1));
}

void enable_usboh3_clk(bool enable)
{
	unsigned int cg = enable ? MXC_CCM_CCGR_CG_ON : MXC_CCM_CCGR_CG_OFF;

	clrsetbits_le32(&mxc_ccm->CCGR2,
			MXC_CCM_CCGR2_USBOH3_60M(MXC_CCM_CCGR_CG_MASK),
			MXC_CCM_CCGR2_USBOH3_60M(cg));
}

void ipu_clk_enable(void)
{
	/* IPU root clock derived from AXI B */
	clrsetbits_le32(&mxc_ccm->cbcmr, MXC_CCM_CBCMR_IPU_HSP_CLK_SEL_MASK,
			MXC_CCM_CBCMR_IPU_HSP_CLK_SEL(1));

	setbits_le32(&mxc_ccm->CCGR5,
		MXC_CCM_CCGR5_IPU(MXC_CCM_CCGR_CG_MASK));

	/* Handshake with IPU when certain clock rates are changed. */
	clrbits_le32(&mxc_ccm->ccdr, MXC_CCM_CCDR_IPU_HS_MASK);

	/* Handshake with IPU when LPM is entered as its enabled. */
	clrbits_le32(&mxc_ccm->clpcr, MXC_CCM_CLPCR_BYPASS_IPU_LPM_HS);
}

void ipu_clk_disable(void)
{
	clrbits_le32(&mxc_ccm->CCGR5,
		MXC_CCM_CCGR5_IPU(MXC_CCM_CCGR_CG_MASK));

	/* Handshake with IPU when certain clock rates are changed. */
	setbits_le32(&mxc_ccm->ccdr, MXC_CCM_CCDR_IPU_HS_MASK);

	/* Handshake with IPU when LPM is entered as its enabled. */
	setbits_le32(&mxc_ccm->clpcr, MXC_CCM_CLPCR_BYPASS_IPU_LPM_HS);
}

void ipu_di_clk_enable(int di)
{
	switch (di) {
	case 0:
		setbits_le32(&mxc_ccm->CCGR6,
			MXC_CCM_CCGR6_IPU_DI0(MXC_CCM_CCGR_CG_MASK));
		break;
	case 1:
		setbits_le32(&mxc_ccm->CCGR6,
			MXC_CCM_CCGR6_IPU_DI1(MXC_CCM_CCGR_CG_MASK));
		break;
	default:
		printf("%s: Invalid DI index %d\n", __func__, di);
	}
}

void ipu_di_clk_disable(int di)
{
	switch (di) {
	case 0:
		clrbits_le32(&mxc_ccm->CCGR6,
			MXC_CCM_CCGR6_IPU_DI0(MXC_CCM_CCGR_CG_MASK));
		break;
	case 1:
		clrbits_le32(&mxc_ccm->CCGR6,
			MXC_CCM_CCGR6_IPU_DI1(MXC_CCM_CCGR_CG_MASK));
		break;
	default:
		printf("%s: Invalid DI index %d\n", __func__, di);
	}
}

#ifdef CONFIG_SOC_MX53
void ldb_clk_enable(int ldb)
{
	switch (ldb) {
	case 0:
		setbits_le32(&mxc_ccm->CCGR6,
			MXC_CCM_CCGR6_LDB_DI0(MXC_CCM_CCGR_CG_MASK));
		break;
	case 1:
		setbits_le32(&mxc_ccm->CCGR6,
			MXC_CCM_CCGR6_LDB_DI1(MXC_CCM_CCGR_CG_MASK));
		break;
	default:
		printf("%s: Invalid LDB index %d\n", __func__, ldb);
	}
}

void ldb_clk_disable(int ldb)
{
	switch (ldb) {
	case 0:
		clrbits_le32(&mxc_ccm->CCGR6,
			MXC_CCM_CCGR6_LDB_DI0(MXC_CCM_CCGR_CG_MASK));
		break;
	case 1:
		clrbits_le32(&mxc_ccm->CCGR6,
			MXC_CCM_CCGR6_LDB_DI1(MXC_CCM_CCGR_CG_MASK));
		break;
	default:
		printf("%s: Invalid LDB index %d\n", __func__, ldb);
	}
}
#endif

#ifdef CONFIG_SYS_I2C_MXC
/* i2c_num can be from 0, to 1 for i.MX51 and 2 for i.MX53 */
int enable_i2c_clk(unsigned char enable, unsigned i2c_num)
{
	u32 mask;

#if defined(CONFIG_SOC_MX51)
	if (i2c_num > 1)
#elif defined(CONFIG_SOC_MX53)
	if (i2c_num > 2)
#endif
		return -EINVAL;
	mask = MXC_CCM_CCGR_CG_MASK <<
			(MXC_CCM_CCGR1_I2C1_OFFSET + (i2c_num << 1));
	if (enable)
		setbits_le32(&mxc_ccm->CCGR1, mask);
	else
		clrbits_le32(&mxc_ccm->CCGR1, mask);
	return 0;
}
#endif

void set_usb_phy_clk(void)
{
	clrbits_le32(&mxc_ccm->cscmr1, MXC_CCM_CSCMR1_USB_PHY_CLK_SEL);
}

#if defined(CONFIG_SOC_MX51)
void enable_usb_phy1_clk(bool enable)
{
	unsigned int cg = enable ? MXC_CCM_CCGR_CG_ON : MXC_CCM_CCGR_CG_OFF;

	clrsetbits_le32(&mxc_ccm->CCGR2,
			MXC_CCM_CCGR2_USB_PHY(MXC_CCM_CCGR_CG_MASK),
			MXC_CCM_CCGR2_USB_PHY(cg));
}

void enable_usb_phy2_clk(bool enable)
{
	/* i.MX51 has a single USB PHY clock, so do nothing here. */
}
#elif defined(CONFIG_SOC_MX53)
void enable_usb_phy1_clk(bool enable)
{
	unsigned int cg = enable ? MXC_CCM_CCGR_CG_ON : MXC_CCM_CCGR_CG_OFF;

	clrsetbits_le32(&mxc_ccm->CCGR4,
			MXC_CCM_CCGR4_USB_PHY1(MXC_CCM_CCGR_CG_MASK),
			MXC_CCM_CCGR4_USB_PHY1(cg));
}

void enable_usb_phy2_clk(bool enable)
{
	unsigned int cg = enable ? MXC_CCM_CCGR_CG_ON : MXC_CCM_CCGR_CG_OFF;

	clrsetbits_le32(&mxc_ccm->CCGR4,
			MXC_CCM_CCGR4_USB_PHY2(MXC_CCM_CCGR_CG_MASK),
			MXC_CCM_CCGR4_USB_PHY2(cg));
}
#endif

/*
 * Calculate the frequency of PLLn.
 */
static uint32_t decode_pll(struct mxc_pll_reg *pll, uint32_t infreq)
{
	uint32_t ctrl, op;
	int mfd, mfn, mfi, pdf, ret;
	uint64_t refclk, temp;
	uint32_t mfn_abs;

	ctrl = readl(&pll->ctrl);

	if (ctrl & MXC_DPLLC_CTL_HFSM) {
		mfn = readl(&pll->hfs_mfn);
		mfd = readl(&pll->hfs_mfd);
		op = readl(&pll->hfs_op);
	} else {
		mfn = readl(&pll->mfn);
		mfd = readl(&pll->mfd);
		op = readl(&pll->op);
	}

	mfd &= MXC_DPLLC_MFD_MFD_MASK;
	mfn &= MXC_DPLLC_MFN_MFN_MASK;
	pdf = op & MXC_DPLLC_OP_PDF_MASK;
	mfi = MXC_DPLLC_OP_MFI_RD(op);

	/* 21.2.3 */
	if (mfi < 5)
		mfi = 5;

	/* Sign extend */
	if (mfn >= 0x04000000) {
		mfn |= 0xfc000000;
		mfn_abs = -mfn;
	} else {
		mfn_abs = mfn;
	}
	refclk = infreq * 2;
	if (ctrl & MXC_DPLLC_CTL_DPDCK0_2_EN)
		refclk *= 2;

	temp = refclk * mfn_abs;
	do_div(temp, mfd + 1);
	ret = refclk * mfi;

	if (mfn < 0)
		ret -= temp;
	else
		ret += temp;

	ret /= pdf + 1;
	return ret;
}

#ifdef CONFIG_SOC_MX51
/*
 * This function returns the Frequency Pre-Multiplier clock.
 */
static u32 get_fpm(void)
{
	u32 mult;
	u32 ccr = readl(&mxc_ccm->ccr);

	if (ccr & MXC_CCM_CCR_FPM_MULT)
		mult = 1024;
	else
		mult = 512;

	return MXC_CLK32 * mult;
}
#endif

/*
 * This function returns the low power audio clock.
 */
static u32 get_lp_apm(void)
{
	u32 ret_val = 0;
	u32 ccsr = readl(&mxc_ccm->ccsr);

	if (ccsr & MXC_CCM_CCSR_LP_APM)
#if defined(CONFIG_SOC_MX51)
		ret_val = get_fpm();
#elif defined(CONFIG_SOC_MX53)
		ret_val = decode_pll(mxc_plls[PLL4_CLOCK], MXC_HCLK);
#endif
	else
		ret_val = MXC_HCLK;

	return ret_val;
}

/*
 * Get mcu main rate
 */
u32 get_mcu_main_clk(void)
{
	u32 reg, freq;

	reg = MXC_CCM_CACRR_ARM_PODF_RD(readl(&mxc_ccm->cacrr));
	freq = decode_pll(mxc_plls[PLL1_CLOCK], MXC_HCLK);
	return freq / (reg + 1);
}

/*
 * Get the rate of peripheral's root clock.
 */
u32 get_periph_clk(void)
{
	u32 reg;

	reg = readl(&mxc_ccm->cbcdr);
	if (!(reg & MXC_CCM_CBCDR_PERIPH_CLK_SEL))
		return decode_pll(mxc_plls[PLL2_CLOCK], MXC_HCLK);
	reg = readl(&mxc_ccm->cbcmr);
	switch (MXC_CCM_CBCMR_PERIPH_CLK_SEL_RD(reg)) {
	case 0:
		return decode_pll(mxc_plls[PLL1_CLOCK], MXC_HCLK);
	case 1:
		return decode_pll(mxc_plls[PLL3_CLOCK], MXC_HCLK);
	case 2:
		return get_lp_apm();
	default:
		return 0;
	}
	/* NOTREACHED */
}

/*
 * Get the rate of ipg clock.
 */
static u32 get_ipg_clk(void)
{
	uint32_t freq, reg, div;

	freq = get_ahb_clk();

	reg = readl(&mxc_ccm->cbcdr);
	div = MXC_CCM_CBCDR_IPG_PODF_RD(reg) + 1;

	return freq / div;
}

/*
 * Get the rate of ipg_per clock.
 */
static u32 get_ipg_per_clk(void)
{
	u32 freq, pred1, pred2, podf;

	if (readl(&mxc_ccm->cbcmr) & MXC_CCM_CBCMR_PERCLK_IPG_CLK_SEL)
		return get_ipg_clk();

	if (readl(&mxc_ccm->cbcmr) & MXC_CCM_CBCMR_PERCLK_LP_APM_CLK_SEL)
		freq = get_lp_apm();
	else
		freq = get_periph_clk();
	podf = readl(&mxc_ccm->cbcdr);
	pred1 = MXC_CCM_CBCDR_PERCLK_PRED1_RD(podf);
	pred2 = MXC_CCM_CBCDR_PERCLK_PRED2_RD(podf);
	podf = MXC_CCM_CBCDR_PERCLK_PODF_RD(podf);
	return freq / ((pred1 + 1) * (pred2 + 1) * (podf + 1));
}

/* Get the output clock rate of a standard PLL MUX for peripherals. */
static u32 get_standard_pll_sel_clk(u32 clk_sel)
{
	u32 freq = 0;

	switch (clk_sel & 0x3) {
	case 0:
		freq = decode_pll(mxc_plls[PLL1_CLOCK], MXC_HCLK);
		break;
	case 1:
		freq = decode_pll(mxc_plls[PLL2_CLOCK], MXC_HCLK);
		break;
	case 2:
		freq = decode_pll(mxc_plls[PLL3_CLOCK], MXC_HCLK);
		break;
	case 3:
		freq = get_lp_apm();
		break;
	}

	return freq;
}

/*
 * Get the rate of uart clk.
 */
static u32 get_uart_clk(void)
{
	unsigned int clk_sel, freq, reg, pred, podf;

	reg = readl(&mxc_ccm->cscmr1);
	clk_sel = MXC_CCM_CSCMR1_UART_CLK_SEL_RD(reg);
	freq = get_standard_pll_sel_clk(clk_sel);

	reg = readl(&mxc_ccm->cscdr1);
	pred = MXC_CCM_CSCDR1_UART_CLK_PRED_RD(reg);
	podf = MXC_CCM_CSCDR1_UART_CLK_PODF_RD(reg);
	freq /= (pred + 1) * (podf + 1);

	return freq;
}

/*
 * get cspi clock rate.
 */
static u32 imx_get_cspiclk(void)
{
	u32 ret_val = 0, pdf, pre_pdf, clk_sel, freq;
	u32 cscmr1 = readl(&mxc_ccm->cscmr1);
	u32 cscdr2 = readl(&mxc_ccm->cscdr2);

	pre_pdf = MXC_CCM_CSCDR2_CSPI_CLK_PRED_RD(cscdr2);
	pdf = MXC_CCM_CSCDR2_CSPI_CLK_PODF_RD(cscdr2);
	clk_sel = MXC_CCM_CSCMR1_CSPI_CLK_SEL_RD(cscmr1);
	freq = get_standard_pll_sel_clk(clk_sel);
	ret_val = freq / ((pre_pdf + 1) * (pdf + 1));
	return ret_val;
}

/*
 * get esdhc clock rate.
 */
static u32 get_esdhc_clk(u32 port)
{
	u32 clk_sel = 0, pred = 0, podf = 0, freq = 0;
	u32 cscmr1 = readl(&mxc_ccm->cscmr1);
	u32 cscdr1 = readl(&mxc_ccm->cscdr1);

	switch (port) {
	case 0:
		clk_sel = MXC_CCM_CSCMR1_ESDHC1_MSHC1_CLK_SEL_RD(cscmr1);
		pred = MXC_CCM_CSCDR1_ESDHC1_MSHC1_CLK_PRED_RD(cscdr1);
		podf = MXC_CCM_CSCDR1_ESDHC1_MSHC1_CLK_PODF_RD(cscdr1);
		break;
	case 1:
		clk_sel = MXC_CCM_CSCMR1_ESDHC2_MSHC2_CLK_SEL_RD(cscmr1);
		pred = MXC_CCM_CSCDR1_ESDHC2_MSHC2_CLK_PRED_RD(cscdr1);
		podf = MXC_CCM_CSCDR1_ESDHC2_MSHC2_CLK_PODF_RD(cscdr1);
		break;
	case 2:
		if (cscmr1 & MXC_CCM_CSCMR1_ESDHC3_CLK_SEL)
			return get_esdhc_clk(1);
		else
			return get_esdhc_clk(0);
	case 3:
		if (cscmr1 & MXC_CCM_CSCMR1_ESDHC4_CLK_SEL)
			return get_esdhc_clk(1);
		else
			return get_esdhc_clk(0);
	default:
		break;
	}

	freq = get_standard_pll_sel_clk(clk_sel) / ((pred + 1) * (podf + 1));
	return freq;
}

static u32 get_axi_a_clk(void)
{
	u32 cbcdr = readl(&mxc_ccm->cbcdr);
	u32 pdf = MXC_CCM_CBCDR_AXI_A_PODF_RD(cbcdr);

	return  get_periph_clk() / (pdf + 1);
}

static u32 get_axi_b_clk(void)
{
	u32 cbcdr = readl(&mxc_ccm->cbcdr);
	u32 pdf = MXC_CCM_CBCDR_AXI_B_PODF_RD(cbcdr);

	return  get_periph_clk() / (pdf + 1);
}

static u32 get_emi_slow_clk(void)
{
	u32 cbcdr = readl(&mxc_ccm->cbcdr);
	u32 emi_clk_sel = cbcdr & MXC_CCM_CBCDR_EMI_CLK_SEL;
	u32 pdf = MXC_CCM_CBCDR_EMI_PODF_RD(cbcdr);

	if (emi_clk_sel)
		return  get_ahb_clk() / (pdf + 1);

	return  get_periph_clk() / (pdf + 1);
}

static u32 get_nfc_clk(void)
{
	u32 parent_rate = get_emi_slow_clk();
	u32 div = readl(&mxc_ccm->cbcdr);

	div &= MXC_CCM_CBCDR_NFC_PODF_MASK;
	div >>= MXC_CCM_CBCDR_NFC_PODF_OFFSET;
	div++;
	return parent_rate / div;
}

static u32 get_ddr_clk(void)
{
	u32 ret_val = 0;
	u32 cbcmr = readl(&mxc_ccm->cbcmr);
	u32 ddr_clk_sel = MXC_CCM_CBCMR_DDR_CLK_SEL_RD(cbcmr);
#ifdef CONFIG_SOC_MX51
	u32 cbcdr = readl(&mxc_ccm->cbcdr);
	if (cbcdr & MXC_CCM_CBCDR_DDR_HIFREQ_SEL) {
		u32 ddr_clk_podf = MXC_CCM_CBCDR_DDR_PODF_RD(cbcdr);

		ret_val = decode_pll(mxc_plls[PLL1_CLOCK], MXC_HCLK);
		ret_val /= ddr_clk_podf + 1;

		return ret_val;
	}
#endif
	switch (ddr_clk_sel) {
	case 0:
		ret_val = get_axi_a_clk();
		break;
	case 1:
		ret_val = get_axi_b_clk();
		break;
	case 2:
		ret_val = get_emi_slow_clk();
		break;
	case 3:
		ret_val = get_ahb_clk();
		break;
	default:
		break;
	}

	return ret_val;
}

/*
 * The API of get mxc clocks.
 */
unsigned int mxc_get_clock(enum mxc_clock clk)
{
	switch (clk) {
	case MXC_ARM_CLK:
		return get_mcu_main_clk();
	case MXC_AHB_CLK:
		return get_ahb_clk();
	case MXC_IPG_CLK:
		return get_ipg_clk();
	case MXC_IPG_PERCLK:
	case MXC_I2C_CLK:
		return get_ipg_per_clk();
	case MXC_UART_CLK:
		return get_uart_clk();
	case MXC_CSPI_CLK:
		return imx_get_cspiclk();
	case MXC_ESDHC_CLK:
		return get_esdhc_clk(0);
	case MXC_ESDHC2_CLK:
		return get_esdhc_clk(1);
	case MXC_ESDHC3_CLK:
		return get_esdhc_clk(2);
	case MXC_ESDHC4_CLK:
		return get_esdhc_clk(3);
	case MXC_FEC_CLK:
		return get_ipg_clk();
	case MXC_SATA_CLK:
		return get_ahb_clk();
	case MXC_DDR_CLK:
		return get_ddr_clk();
	case MXC_AXI_A_CLK:
		return get_axi_a_clk();
	case MXC_AXI_B_CLK:
		return get_axi_b_clk();
	case MXC_EMI_SLOW_CLK:
		return get_emi_slow_clk();
	case MXC_NFC_CLK:
		return get_nfc_clk();
	default:
		break;
	}
	return -EINVAL;
}

u32 imx_get_uartclk(void)
{
	return get_uart_clk();
}

u32 imx_get_fecclk(void)
{
	return get_ipg_clk();
}

static int gcd(int m, int n)
{
	int t;
	while (m > 0) {
		if (n > m) {
			t = m;
			m = n;
			n = t;
		} /* swap */
		m -= n;
	}
	return n;
}

/*
 * This is to calculate various parameters based on reference clock and
 * targeted clock based on the equation:
 *      t_clk = 2*ref_freq*(mfi + mfn/(mfd+1))/(pd+1)
 * This calculation is based on a fixed MFD value for simplicity.
 */
static int calc_pll_params(u32 ref, u32 target, struct pll_param *pll)
{
	int pd, mfi = 1, mfn, mfd;
	u64 t1;
	size_t i;

	/*
	 * Make sure targeted freq is in the valid range.
	 * Otherwise the following calculation might be wrong!!!
	 */
	if (target < PLL_FREQ_MIN(ref) ||
		target > PLL_FREQ_MAX(ref)) {
		printf("Targeted pll clock should be within [%d - %d]\n",
			PLL_FREQ_MIN(ref) / SZ_DEC_1M,
			PLL_FREQ_MAX(ref) / SZ_DEC_1M);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(fixed_mfd); i++) {
		if (fixed_mfd[i].ref_clk_hz == ref) {
			mfd = fixed_mfd[i].mfd;
			break;
		}
	}

	if (i == ARRAY_SIZE(fixed_mfd))
		return -EINVAL;

	for (pd = 1; pd <= PLL_PD_MAX; pd++) {
		t1 = (u64)target * pd;
		do_div(t1, (4 * ref));
		mfi = t1;
		if (mfi > PLL_MFI_MAX)
			return -EINVAL;
		else if (mfi < 5)
			continue;
		break;
	}
	/*
	 * Now got pd and mfi already
	 *
	 * mfn = (((target * pd) / 4 - ref * mfi) * mfd) / ref;
	 */
	t1 = (u64)target * pd;
	do_div(t1, 4);
	t1 = (t1 - ref * mfi) * mfd;
	do_div(t1, ref);
	mfn = t1;
	if (mfn != 0) {
		i = gcd(mfd, mfn);
		mfn /= i;
		mfd /= i;
	} else {
		mfd = 1;
	}
	debug("ref=%d, target=%d, pd=%d, mfi=%d, mfn=%d, mfd=%d\n",
		ref, target, pd, mfi, mfn, mfd);
	pll->pd = pd;
	pll->mfi = mfi;
	pll->mfn = mfn;
	pll->mfd = mfd;

	return 0;
}

#define calc_div(tgt_clk, src_clk, limit) ({		\
		u32 v = 0;				\
		if (((src_clk) % (tgt_clk)) <= 100)	\
			v = (src_clk) / (tgt_clk);	\
		else					\
			v = ((src_clk) / (tgt_clk)) + 1;\
		if (v > limit)				\
			v = limit;			\
		(v - 1);				\
	})

#define CHANGE_PLL_SETTINGS(pll, pd, fi, fn, fd) \
	{	\
		__raw_writel(0x1232, &pll->ctrl);		\
		__raw_writel(0x2, &pll->config);		\
		__raw_writel((((pd) - 1) << 0) | ((fi) << 4),	\
			&pll->op);				\
		__raw_writel(fn, &(pll->mfn));			\
		__raw_writel((fd) - 1, &pll->mfd);		\
		__raw_writel((((pd) - 1) << 0) | ((fi) << 4),	\
			&pll->hfs_op);				\
		__raw_writel(fn, &pll->hfs_mfn);		\
		__raw_writel((fd) - 1, &pll->hfs_mfd);		\
		__raw_writel(0x1232, &pll->ctrl);		\
		while (!__raw_readl(&pll->ctrl) & 0x1)		\
			;\
	}

static int config_pll_clk(enum pll_clocks index, struct pll_param *pll_param)
{
	u32 ccsr = __raw_readl(&mxc_ccm->ccsr);
	struct mxc_pll_reg *pll = mxc_plls[index];

	switch (index) {
	case PLL1_CLOCK:
		/* Switch ARM to PLL2 clock */
		__raw_writel(ccsr | 0x4, &mxc_ccm->ccsr);
		CHANGE_PLL_SETTINGS(pll, pll_param->pd,
					pll_param->mfi, pll_param->mfn,
					pll_param->mfd);
		/* Switch back */
		__raw_writel(ccsr & ~0x4, &mxc_ccm->ccsr);
		break;
	case PLL2_CLOCK:
		/* Switch to pll2 bypass clock */
		__raw_writel(ccsr | 0x2, &mxc_ccm->ccsr);
		CHANGE_PLL_SETTINGS(pll, pll_param->pd,
					pll_param->mfi, pll_param->mfn,
					pll_param->mfd);
		/* Switch back */
		__raw_writel(ccsr & ~0x2, &mxc_ccm->ccsr);
		break;
	case PLL3_CLOCK:
		/* Switch to pll3 bypass clock */
		__raw_writel(ccsr | 0x1, &mxc_ccm->ccsr);
		CHANGE_PLL_SETTINGS(pll, pll_param->pd,
					pll_param->mfi, pll_param->mfn,
					pll_param->mfd);
		/* Switch back */
		__raw_writel(ccsr & ~0x1, &mxc_ccm->ccsr);
		break;
#ifdef CONFIG_SOC_MX53
	case PLL4_CLOCK:
		/* Switch to pll4 bypass clock */
		__raw_writel(ccsr | 0x20, &mxc_ccm->ccsr);
		CHANGE_PLL_SETTINGS(pll, pll_param->pd,
					pll_param->mfi, pll_param->mfn,
					pll_param->mfd);
		/* Switch back */
		__raw_writel(ccsr & ~0x20, &mxc_ccm->ccsr);
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int __adjust_core_voltage_stub(u32 freq)
{
	return 0;
}
int adjust_core_voltage(u32 freq)
	__attribute__((weak, alias("__adjust_core_voltage_stub")));

/* Config CPU clock */
static int config_core_clk(u32 ref, u32 freq)
{
	int ret = 0;
	struct pll_param pll_param;
	u32 cur_freq = decode_pll(mxc_plls[PLL1_CLOCK], MXC_HCLK);

	if (freq == cur_freq)
		return 0;

	memset(&pll_param, 0, sizeof(struct pll_param));

	/* The case that periph uses PLL1 is not considered here */
	ret = calc_pll_params(ref, freq, &pll_param);
	if (ret != 0) {
		printf("Error: Can't find pll parameters for %u.%03uMHz ref %u.%03uMHz\n",
			freq / 1000000, freq / 1000 % 1000,
			ref / 1000000, ref / 1000 % 1000);
		return ret;
	}
	if (freq > cur_freq) {
		ret = adjust_core_voltage(freq);
		if (ret < 0) {
			printf("Failed to adjust core voltage for changing ARM clk from %u.%03uMHz to  %u.%03uMHz\n",
				cur_freq / 1000000, cur_freq / 1000 % 1000,
				freq / 1000000, freq / 1000 % 1000);
			return ret;
		}
		ret = config_pll_clk(PLL1_CLOCK, &pll_param);
		if (ret) {
			adjust_core_voltage(cur_freq);
		}
	} else {
		ret = config_pll_clk(PLL1_CLOCK, &pll_param);
		if (ret) {
			return ret;
		}
		ret = adjust_core_voltage(freq);
		if (ret < 0) {
			printf("Failed to adjust core voltage for changing ARM clk from %u.%03uMHz to  %u.%03uMHz\n",
				cur_freq / 1000000, cur_freq / 1000 % 1000,
				freq / 1000000, freq / 1000 % 1000);
			calc_pll_params(ref, cur_freq, &pll_param);
			config_pll_clk(PLL1_CLOCK, &pll_param);
		}
	}
	return ret;
}

static int config_nfc_clk(u32 nfc_clk)
{
	u32 parent_rate = get_emi_slow_clk();
	u32 div;

	if (nfc_clk == 0)
		return -EINVAL;
	div = parent_rate / nfc_clk;
	if (div == 0)
		div++;
	if (parent_rate / div > NFC_CLK_MAX)
		div++;
	clrsetbits_le32(&mxc_ccm->cbcdr,
			MXC_CCM_CBCDR_NFC_PODF_MASK,
			MXC_CCM_CBCDR_NFC_PODF(div - 1));
	while (readl(&mxc_ccm->cdhipr) != 0)
		;
	return 0;
}

void enable_nfc_clk(unsigned char enable)
{
	unsigned int cg = enable ? MXC_CCM_CCGR_CG_ON : MXC_CCM_CCGR_CG_OFF;

	clrsetbits_le32(&mxc_ccm->CCGR5,
		MXC_CCM_CCGR5_EMI_ENFC(MXC_CCM_CCGR_CG_MASK),
		MXC_CCM_CCGR5_EMI_ENFC(cg));
}

#ifdef CONFIG_FSL_IIM
void enable_efuse_prog_supply(bool enable)
{
	if (enable)
		setbits_le32(&mxc_ccm->cgpr,
			     MXC_CCM_CGPR_EFUSE_PROG_SUPPLY_GATE);
	else
		clrbits_le32(&mxc_ccm->cgpr,
			     MXC_CCM_CGPR_EFUSE_PROG_SUPPLY_GATE);
}
#endif

/* Config main_bus_clock for periphs */
static int config_periph_clk(u32 ref, u32 freq)
{
	int ret = 0;
	struct pll_param pll_param;

	memset(&pll_param, 0, sizeof(struct pll_param));

	if (readl(&mxc_ccm->cbcdr) & MXC_CCM_CBCDR_PERIPH_CLK_SEL) {
		ret = calc_pll_params(ref, freq, &pll_param);
		if (ret != 0) {
			printf("Error:Can't find pll parameters: %d\n",
				ret);
			return ret;
		}
		switch (MXC_CCM_CBCMR_PERIPH_CLK_SEL_RD(
				readl(&mxc_ccm->cbcmr))) {
		case 0:
			return config_pll_clk(PLL1_CLOCK, &pll_param);
			break;
		case 1:
			return config_pll_clk(PLL3_CLOCK, &pll_param);
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int config_ddr_clk(u32 emi_clk)
{
	u32 clk_src;
	s32 shift = 0, clk_sel, div = 1;
	u32 cbcmr = readl(&mxc_ccm->cbcmr);

	if (emi_clk > MAX_DDR_CLK) {
		printf("Warning:DDR clock should not exceed %d MHz\n",
			MAX_DDR_CLK / SZ_DEC_1M);
		emi_clk = MAX_DDR_CLK;
	}

	clk_src = get_periph_clk();
	/* Find DDR clock input */
	clk_sel = MXC_CCM_CBCMR_DDR_CLK_SEL_RD(cbcmr);
	switch (clk_sel) {
	case 0:
		shift = 16;
		break;
	case 1:
		shift = 19;
		break;
	case 2:
		shift = 22;
		break;
	case 3:
		shift = 10;
		break;
	default:
		return -EINVAL;
	}

	if ((clk_src % emi_clk) < 10000000)
		div = clk_src / emi_clk;
	else
		div = (clk_src / emi_clk) + 1;
	if (div > 8)
		div = 8;

	clrsetbits_le32(&mxc_ccm->cbcdr, 0x7 << shift, (div - 1) << shift);
	while (readl(&mxc_ccm->cdhipr) != 0)
		;
	writel(0x0, &mxc_ccm->ccdr);

	return 0;
}

/*
 * This function assumes the expected core clock has to be changed by
 * modifying the PLL. This is NOT true always but for most of the times,
 * it is. So it assumes the PLL output freq is the same as the expected
 * core clock (presc=1) unless the core clock is less than PLL_FREQ_MIN.
 * In the latter case, it will try to increase the presc value until
 * (presc*core_clk) is greater than PLL_FREQ_MIN. It then makes call to
 * calc_pll_params() and obtains the values of PD, MFI,MFN, MFD based
 * on the targeted PLL and reference input clock to the PLL. Lastly,
 * it sets the register based on these values along with the dividers.
 * Note 1) There is no value checking for the passed-in divider values
 *         so the caller has to make sure those values are sensible.
 *      2) Also adjust the NFC divider such that the NFC clock doesn't
 *         exceed NFC_CLK_MAX.
 *      3) IPU HSP clock is independent of AHB clock. Even it can go up to
 *         177MHz for higher voltage, this function fixes the max to 133MHz.
 *      4) This function should not have allowed diag_printf() calls since
 *         the serial driver has been stoped. But leave then here to allow
 *         easy debugging by NOT calling the cyg_hal_plf_serial_stop().
 */
int mxc_set_clock(u32 ref, u32 freq, enum mxc_clock clk)
{
	freq *= SZ_DEC_1M;

	switch (clk) {
	case MXC_ARM_CLK:
		if (config_core_clk(ref, freq))
			return -EINVAL;
		break;
	case MXC_PERIPH_CLK:
		if (config_periph_clk(ref, freq))
			return -EINVAL;
		break;
	case MXC_DDR_CLK:
		if (config_ddr_clk(freq))
			return -EINVAL;
		break;
	case MXC_NFC_CLK:
		if (config_nfc_clk(freq))
			return -EINVAL;
		break;
	default:
		printf("Warning:Unsupported or invalid clock type\n");
	}

	return 0;
}

#ifdef CONFIG_SOC_MX53
/*
 * The clock for the external interface can be set to use internal clock
 * if fuse bank 4, row 3, bit 2 is set.
 * This is an undocumented feature and it was confirmed by Freescale's support:
 * Fuses (but not pins) may be used to configure SATA clocks.
 * Particularly the i.MX53 Fuse_Map contains the next information
 * about configuring SATA clocks :  SATA_ALT_REF_CLK[1:0] (offset 0x180C)
 * '00' - 100MHz (External)
 * '01' - 50MHz (External)
 * '10' - 120MHz, internal (USB PHY)
 * '11' - Reserved
*/
void mxc_set_sata_internal_clock(void)
{
	u32 *tmp_base =
		(u32 *)(IIM_BASE_ADDR + 0x180c);

	set_usb_phy_clk();

	clrsetbits_le32(tmp_base, 0x6, 0x4);
}
#endif

/*
 * Dump some core clockes.
 */
#define pr_clk_val(c, v) {					\
	printf("%-11s %3lu.%03lu MHz\n", #c,			\
		(v) / 1000000, (v) / 1000 % 1000);		\
}

#define pr_clk(c) {						\
	unsigned long __clk = mxc_get_clock(MXC_##c##_CLK);	\
	pr_clk_val(c, __clk);					\
}

static int do_mx5_showclocks(void)
{
	unsigned long freq;

	freq = decode_pll(mxc_plls[PLL1_CLOCK], MXC_HCLK);
	pr_clk_val(PLL1, freq);
	freq = decode_pll(mxc_plls[PLL2_CLOCK], MXC_HCLK);
	pr_clk_val(PLL2, freq);
	freq = decode_pll(mxc_plls[PLL3_CLOCK], MXC_HCLK);
	pr_clk_val(PLL3, freq);
#ifdef	CONFIG_SOC_MX53
	freq = decode_pll(mxc_plls[PLL4_CLOCK], MXC_HCLK);
	pr_clk_val(PLL4, freq);
#endif

	printf("\n");
	pr_clk(AHB);
	pr_clk(AXI_A);
	pr_clk(AXI_B);
	pr_clk(IPG);
	pr_clk(IPG_PER);
	pr_clk(DDR);
	pr_clk(EMI_SLOW);
	pr_clk(NFC);
#ifdef CONFIG_MXC_SPI
	pr_clk(CSPI);
#endif
	return 0;
}

static struct clk_lookup {
	const char *name;
	unsigned int index;
} mx5_clk_lookup[] = {
	{ "arm", MXC_ARM_CLK, },
};

int do_clocks(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int i;
	unsigned long freq;
	unsigned long ref = ~0UL;

	if (argc < 2) {
		do_mx5_showclocks();
		return CMD_RET_SUCCESS;
	} else if (argc == 2 || argc > 4) {
		return CMD_RET_USAGE;
	}

	freq = simple_strtoul(argv[2], NULL, 0);
	if (freq == 0) {
		printf("Invalid clock frequency %lu\n", freq);
		return CMD_RET_FAILURE;
	}
	if (argc > 3) {
		ref = simple_strtoul(argv[3], NULL, 0);
	}
	for (i = 0; i < ARRAY_SIZE(mx5_clk_lookup); i++) {
		if (strcasecmp(argv[1], mx5_clk_lookup[i].name) == 0) {
			switch (mx5_clk_lookup[i].index) {
			case MXC_ARM_CLK:
				if (argc > 3)
					return CMD_RET_USAGE;
				ref = CONFIG_SYS_MX5_HCLK;
				break;

			case MXC_NFC_CLK:
				if (argc > 3 && ref > 3) {
					printf("Invalid clock selector value: %lu\n", ref);
					return CMD_RET_FAILURE;
				}
				break;
			}
			printf("Setting %s clock to %lu MHz\n",
				mx5_clk_lookup[i].name, freq);
			if (mxc_set_clock(ref, freq, mx5_clk_lookup[i].index))
				break;
			freq = mxc_get_clock(mx5_clk_lookup[i].index);
			printf("%s clock set to %lu.%03lu MHz\n",
				mx5_clk_lookup[i].name,
				freq / 1000000, freq / 1000 % 1000);
			return CMD_RET_SUCCESS;
		}
	}
	if (i == ARRAY_SIZE(mx5_clk_lookup)) {
		printf("clock %s not found; supported clocks are:\n", argv[1]);
		for (i = 0; i < ARRAY_SIZE(mx5_clk_lookup); i++) {
			printf("\t%s\n", mx5_clk_lookup[i].name);
		}
	} else {
		printf("Failed to set clock %s to %s MHz\n",
			argv[1], argv[2]);
	}
	return CMD_RET_FAILURE;
}

/***************************************************/

U_BOOT_CMD(
	clocks,	4, 0, do_clocks,
	"display/set clocks",
	"                    - display clock settings\n"
	"clocks <clkname> <freq>    - set clock <clkname> to <freq> MHz"
);
