/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>

enum pll_clocks {
	PLL_ARM,	/* PLL1: ARM PLL */
	PLL_BUS,	/* PLL2: System Bus PLL*/
	PLL_USBOTG,	/* PLL3: OTG USB PLL */
	PLL_AUDIO,	/* PLL4: Audio PLL */
	PLL_VIDEO,	/* PLL5: Video PLL */
	PLL_ENET,	/* PLL6: ENET PLL */
	PLL_USB2,	/* PLL7: USB2 PLL */
	PLL_MLB,	/* PLL8: MLB PLL */
};

struct mxc_ccm_reg *const imx_ccm = (void *)CCM_BASE_ADDR;
struct anatop_regs *const anatop = (void *)ANATOP_BASE_ADDR;

int clk_enable(struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return 0;
	if (clk->usecount == 0) {
debug("%s: Enabling %s clock\n", __func__, clk->name);
		ret = clk->enable(clk);
		if (ret)
			return ret;
		clk->usecount++;
	}
	assert(clk->usecount > 0);
	return ret;
}

void clk_disable(struct clk *clk)
{
	if (!clk)
		return;

	assert(clk->usecount > 0);
	if (!(--clk->usecount)) {
		if (clk->disable) {
debug("%s: Disabling %s clock\n", __func__, clk->name);
			clk->disable(clk);
		}
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

#ifdef CONFIG_MXC_OCOTP
void enable_ocotp_clk(unsigned char enable)
{
	u32 reg;

	reg = __raw_readl(&imx_ccm->CCGR2);
	if (enable)
		reg |= MXC_CCM_CCGR2_OCOTP_CTRL_MASK;
	else
		reg &= ~MXC_CCM_CCGR2_OCOTP_CTRL_MASK;
	__raw_writel(reg, &imx_ccm->CCGR2);
}
#endif

void enable_usboh3_clk(unsigned char enable)
{
	u32 reg;

	reg = __raw_readl(&imx_ccm->CCGR6);
	if (enable)
		reg |= MXC_CCM_CCGR6_USBOH3_MASK;
	else
		reg &= ~(MXC_CCM_CCGR6_USBOH3_MASK);
	__raw_writel(reg, &imx_ccm->CCGR6);

}

#ifdef CONFIG_I2C_MXC
/* i2c_num can be from 0 - 2 */
int enable_i2c_clk(unsigned char enable, unsigned i2c_num)
{
	u32 reg;
	u32 mask;

	if (i2c_num > 2)
		return -EINVAL;

	mask = MXC_CCM_CCGR_CG_MASK
		<< (MXC_CCM_CCGR2_I2C1_SERIAL_OFFSET + (i2c_num << 1));
	reg = __raw_readl(&imx_ccm->CCGR2);
	if (enable)
		reg |= mask;
	else
		reg &= ~mask;
	__raw_writel(reg, &imx_ccm->CCGR2);
	return 0;
}
#endif

static u32 decode_pll(enum pll_clocks pll, u32 infreq)
{
	u32 div;

	switch (pll) {
	case PLL_ARM:
		div = __raw_readl(&anatop->pll_arm);
		if (div & BM_ANADIG_PLL_ARM_BYPASS)
			/* Assume the bypass clock is always derived from OSC */
			return infreq;
		div &= BM_ANADIG_PLL_ARM_DIV_SELECT;

		return infreq * div / 2;
	case PLL_BUS:
		div = __raw_readl(&anatop->pll_528);
		if (div & BM_ANADIG_PLL_SYS_BYPASS)
			return infreq;
		div &= BM_ANADIG_PLL_SYS_DIV_SELECT;

		return infreq * (20 + div * 2);
	case PLL_USBOTG:
		div = __raw_readl(&anatop->usb1_pll_480_ctrl);
		if (div & BM_ANADIG_USB1_PLL_480_CTRL_BYPASS)
			return infreq;
		div &= BM_ANADIG_USB1_PLL_480_CTRL_DIV_SELECT;

		return infreq * (20 + div * 2);
	case PLL_AUDIO:
		div = __raw_readl(&anatop->pll_audio);
		if (div & BM_ANADIG_PLL_AUDIO_BYPASS)
			return infreq;
		div &= BM_ANADIG_PLL_AUDIO_DIV_SELECT;

		return infreq * div;
	case PLL_VIDEO:
		div = __raw_readl(&anatop->pll_video);
		if (div & BM_ANADIG_PLL_VIDEO_BYPASS)
			return infreq;
		div &= BM_ANADIG_PLL_VIDEO_DIV_SELECT;

		return infreq * div;
	case PLL_ENET:
		div = __raw_readl(&anatop->pll_enet);
		if (div & BM_ANADIG_PLL_ENET_BYPASS)
			return infreq;
		div &= BM_ANADIG_PLL_ENET_DIV_SELECT;

		return (div == 3 ? 125000000 : 25000000 * div * 2);
	case PLL_USB2:
		div = __raw_readl(&anatop->usb2_pll_480_ctrl);
		if (div & BM_ANADIG_USB2_PLL_480_CTRL_BYPASS)
			return infreq;
		div &= BM_ANADIG_USB2_PLL_480_CTRL_DIV_SELECT;

		return infreq * (20 + div * 2);
	case PLL_MLB:
		div = __raw_readl(&anatop->pll_mlb);
		if (div & BM_ANADIG_PLL_MLB_BYPASS)
			return infreq;
		/* unknown external clock provided on MLB_CLK pin */
		return 0;
	}
	return 0;
}

static u32 get_mcu_main_clk(void)
{
	u32 reg, freq;

	reg = __raw_readl(&imx_ccm->cacrr);
	reg &= MXC_CCM_CACRR_ARM_PODF_MASK;
	reg >>= MXC_CCM_CACRR_ARM_PODF_OFFSET;
	freq = decode_pll(PLL_ARM, MXC_HCLK);

	return freq / (reg + 1);
}

u32 get_periph_clk(void)
{
	u32 reg, freq = 0;

	reg = __raw_readl(&imx_ccm->cbcdr);
	if (reg & MXC_CCM_CBCDR_PERIPH_CLK_SEL) {
		reg = __raw_readl(&imx_ccm->cbcmr);
		reg &= MXC_CCM_CBCMR_PERIPH_CLK2_SEL_MASK;
		reg >>= MXC_CCM_CBCMR_PERIPH_CLK2_SEL_OFFSET;

		switch (reg) {
		case 0:
			freq = decode_pll(PLL_USBOTG, MXC_HCLK);
			break;
		case 1:
		case 2:
			freq = MXC_HCLK;
			break;
		}
	} else {
		reg = __raw_readl(&imx_ccm->cbcmr);
		reg &= MXC_CCM_CBCMR_PRE_PERIPH_CLK_SEL_MASK;
		reg >>= MXC_CCM_CBCMR_PRE_PERIPH_CLK_SEL_OFFSET;

		switch (reg) {
		case 0:
			freq = decode_pll(PLL_BUS, MXC_HCLK);
			break;
		case 1:
			freq = PLL2_PFD2_FREQ;
			break;
		case 2:
			freq = PLL2_PFD0_FREQ;
			break;
		case 3:
			freq = PLL2_PFD2_DIV_FREQ;
			break;
		}
	}

	return freq;
}

static u32 get_ipg_clk(void)
{
	u32 reg, ipg_podf;

	reg = __raw_readl(&imx_ccm->cbcdr);
	reg &= MXC_CCM_CBCDR_IPG_PODF_MASK;
	ipg_podf = reg >> MXC_CCM_CBCDR_IPG_PODF_OFFSET;

	return get_ahb_clk() / (ipg_podf + 1);
}

static u32 get_ipg_per_clk(void)
{
	u32 reg, perclk_podf;

	reg = __raw_readl(&imx_ccm->cscmr1);
	perclk_podf = reg & MXC_CCM_CSCMR1_PERCLK_PODF_MASK;

	return get_ipg_clk() / (perclk_podf + 1);
}

static u32 get_uart_clk(void)
{
	u32 reg, uart_podf;
	u32 freq = PLL3_80M;
	reg = __raw_readl(&imx_ccm->cscdr1);
#ifdef CONFIG_MX6SL
	if (reg & MXC_CCM_CSCDR1_UART_CLK_SEL)
		freq = MXC_HCLK;
#endif
	reg &= MXC_CCM_CSCDR1_UART_CLK_PODF_MASK;
	uart_podf = reg >> MXC_CCM_CSCDR1_UART_CLK_PODF_OFFSET;

	return freq / (uart_podf + 1);
}

static u32 get_cspi_clk(void)
{
	u32 reg, cspi_podf;

	reg = __raw_readl(&imx_ccm->cscdr2);
	reg &= MXC_CCM_CSCDR2_ECSPI_CLK_PODF_MASK;
	cspi_podf = reg >> MXC_CCM_CSCDR2_ECSPI_CLK_PODF_OFFSET;

	return	PLL3_60M / (cspi_podf + 1);
}

static u32 get_axi_clk(void)
{
	u32 root_freq, axi_podf;
	u32 cbcdr =  __raw_readl(&imx_ccm->cbcdr);

	axi_podf = cbcdr & MXC_CCM_CBCDR_AXI_PODF_MASK;
	axi_podf >>= MXC_CCM_CBCDR_AXI_PODF_OFFSET;

	if (cbcdr & MXC_CCM_CBCDR_AXI_SEL) {
		if (cbcdr & MXC_CCM_CBCDR_AXI_ALT_SEL)
			root_freq = PLL2_PFD2_FREQ;
		else
			root_freq = PLL3_PFD1_FREQ;
	} else
		root_freq = get_periph_clk();

	return  root_freq / (axi_podf + 1);
}

static u32 get_emi_slow_clk(void)
{
	u32 emi_clk_sel, emi_slow_pof, cscmr1, root_freq = 0;

	cscmr1 =  __raw_readl(&imx_ccm->cscmr1);
	emi_clk_sel = cscmr1 & MXC_CCM_CSCMR1_ACLK_EMI_SLOW_MASK;
	emi_clk_sel >>= MXC_CCM_CSCMR1_ACLK_EMI_SLOW_OFFSET;
	emi_slow_pof = cscmr1 & MXC_CCM_CSCMR1_ACLK_EMI_SLOW_PODF_MASK;
	emi_slow_pof >>= MXC_CCM_CSCMR1_ACLK_EMI_PODF_OFFSET;

	switch (emi_clk_sel) {
	case 0:
		root_freq = get_axi_clk();
		break;
	case 1:
		root_freq = decode_pll(PLL_USBOTG, MXC_HCLK);
		break;
	case 2:
		root_freq = PLL2_PFD2_FREQ;
		break;
	case 3:
		root_freq = PLL2_PFD0_FREQ;
		break;
	}

	return root_freq / (emi_slow_pof + 1);
}

static u32 get_nfc_clk(void)
{
	u32 cs2cdr = __raw_readl(&imx_ccm->cs2cdr);
	u32 podf = (cs2cdr & MXC_CCM_CS2CDR_ENFC_CLK_PODF_MASK) >> MXC_CCM_CS2CDR_ENFC_CLK_PODF_OFFSET;
	u32 pred = (cs2cdr & MXC_CCM_CS2CDR_ENFC_CLK_PRED_MASK) >> MXC_CCM_CS2CDR_ENFC_CLK_PRED_OFFSET;
	int nfc_clk_sel = (cs2cdr & MXC_CCM_CS2CDR_ENFC_CLK_SEL_MASK) >>
		MXC_CCM_CS2CDR_ENFC_CLK_SEL_OFFSET;
	u32 root_freq;

	switch (nfc_clk_sel) {
	case 0:
		root_freq = PLL2_PFD0_FREQ;
		break;
	case 1:
		root_freq = decode_pll(PLL_BUS, MXC_HCLK);
		break;
	case 2:
		root_freq = decode_pll(PLL_USBOTG, MXC_HCLK);
		break;
	case 3:
		root_freq = PLL2_PFD2_FREQ;
		break;
	}

	return root_freq / (pred + 1) / (podf + 1);
}

#define CS2CDR_ENFC_MASK	(MXC_CCM_CS2CDR_ENFC_CLK_PODF_MASK |	\
				MXC_CCM_CS2CDR_ENFC_CLK_PRED_MASK |	\
				MXC_CCM_CS2CDR_ENFC_CLK_SEL_MASK)

static int set_nfc_clk(u32 ref, u32 freq_khz)
{
	u32 cs2cdr = __raw_readl(&imx_ccm->cs2cdr);
	u32 podf;
	u32 pred;
	int nfc_clk_sel;
	u32 root_freq;
	u32 min_err = ~0;
	u32 nfc_val = ~0;
	u32 freq = freq_khz * 1000;

	for (nfc_clk_sel = 0; nfc_clk_sel < 4; nfc_clk_sel++) {
		u32 act_freq;
		u32 err;

		if (ref < 4 && ref != nfc_clk_sel)
			continue;

		switch (nfc_clk_sel) {
		case 0:
			root_freq = PLL2_PFD0_FREQ;
			break;
		case 1:
			root_freq = decode_pll(PLL_BUS, MXC_HCLK);
			break;
		case 2:
			root_freq = decode_pll(PLL_USBOTG, MXC_HCLK);
			break;
		case 3:
			root_freq = PLL2_PFD2_FREQ;
			break;
		}
		if (root_freq < freq)
			continue;

		podf = min(DIV_ROUND_UP(root_freq, freq), 1 << 6);
		pred = min(DIV_ROUND_UP(root_freq / podf, freq), 8);
		act_freq = root_freq / pred / podf;
		err = (freq - act_freq) * 100 / freq;
		debug("root=%d[%u] freq=%u pred=%u podf=%u act=%u err=%d\n",
			nfc_clk_sel, root_freq, freq, pred, podf, act_freq, err);
		if (act_freq > freq)
			continue;
		if (err < min_err) {
			nfc_val = (podf - 1) << MXC_CCM_CS2CDR_ENFC_CLK_PODF_OFFSET;
			nfc_val |= (pred - 1) << MXC_CCM_CS2CDR_ENFC_CLK_PRED_OFFSET;
			nfc_val |= nfc_clk_sel << MXC_CCM_CS2CDR_ENFC_CLK_SEL_OFFSET;
			min_err = err;
			if (err == 0)
				break;
		}
	}

	if (nfc_val == ~0 || min_err > 10)
		return -EINVAL;

	if ((cs2cdr & CS2CDR_ENFC_MASK) != nfc_val) {
		debug("changing cs2cdr from %08x to %08x\n", cs2cdr,
			(cs2cdr & ~CS2CDR_ENFC_MASK) | nfc_val);
		__raw_writel((cs2cdr & ~CS2CDR_ENFC_MASK) | nfc_val,
			&imx_ccm->cs2cdr);
	} else {
		debug("Leaving cs2cdr unchanged [%08x]\n", cs2cdr);
	}
	return 0;
}

#ifdef CONFIG_MX6SL
static u32 get_mmdc_ch0_clk(void)
{
	u32 cbcmr = __raw_readl(&imx_ccm->cbcmr);
	u32 cbcdr = __raw_readl(&imx_ccm->cbcdr);
	u32 freq, podf;

	podf = (cbcdr & MXC_CCM_CBCDR_MMDC_CH1_PODF_MASK) \
			>> MXC_CCM_CBCDR_MMDC_CH1_PODF_OFFSET;

	switch ((cbcmr & MXC_CCM_CBCMR_PRE_PERIPH2_CLK_SEL_MASK) >>
		MXC_CCM_CBCMR_PRE_PERIPH2_CLK_SEL_OFFSET) {
	case 0:
		freq = decode_pll(PLL_BUS, MXC_HCLK);
		break;
	case 1:
		freq = PLL2_PFD2_FREQ;
		break;
	case 2:
		freq = PLL2_PFD0_FREQ;
		break;
	case 3:
		freq = PLL2_PFD2_DIV_FREQ;
	}

	return freq / (podf + 1);

}
#else
static u32 get_mmdc_ch0_clk(void)
{
	u32 cbcdr = __raw_readl(&imx_ccm->cbcdr);
	u32 mmdc_ch0_podf = (cbcdr & MXC_CCM_CBCDR_MMDC_CH0_PODF_MASK) >>
				MXC_CCM_CBCDR_MMDC_CH0_PODF_OFFSET;

	return get_periph_clk() / (mmdc_ch0_podf + 1);
}
#endif

static u32 get_usdhc_clk(u32 port)
{
	u32 root_freq = 0, usdhc_podf = 0, clk_sel = 0;
	u32 cscmr1 = __raw_readl(&imx_ccm->cscmr1);
	u32 cscdr1 = __raw_readl(&imx_ccm->cscdr1);

	switch (port) {
	case 0:
		usdhc_podf = (cscdr1 & MXC_CCM_CSCDR1_USDHC1_PODF_MASK) >>
					MXC_CCM_CSCDR1_USDHC1_PODF_OFFSET;
		clk_sel = cscmr1 & MXC_CCM_CSCMR1_USDHC1_CLK_SEL;

		break;
	case 1:
		usdhc_podf = (cscdr1 & MXC_CCM_CSCDR1_USDHC2_PODF_MASK) >>
					MXC_CCM_CSCDR1_USDHC2_PODF_OFFSET;
		clk_sel = cscmr1 & MXC_CCM_CSCMR1_USDHC2_CLK_SEL;

		break;
	case 2:
		usdhc_podf = (cscdr1 & MXC_CCM_CSCDR1_USDHC3_PODF_MASK) >>
					MXC_CCM_CSCDR1_USDHC3_PODF_OFFSET;
		clk_sel = cscmr1 & MXC_CCM_CSCMR1_USDHC3_CLK_SEL;

		break;
	case 3:
		usdhc_podf = (cscdr1 & MXC_CCM_CSCDR1_USDHC4_PODF_MASK) >>
					MXC_CCM_CSCDR1_USDHC4_PODF_OFFSET;
		clk_sel = cscmr1 & MXC_CCM_CSCMR1_USDHC4_CLK_SEL;

		break;
	default:
		break;
	}

	if (clk_sel)
		root_freq = PLL2_PFD0_FREQ;
	else
		root_freq = PLL2_PFD2_FREQ;

	return root_freq / (usdhc_podf + 1);
}

u32 imx_get_uartclk(void)
{
	return get_uart_clk();
}

u32 imx_get_fecclk(void)
{
	return decode_pll(PLL_ENET, MXC_HCLK);
}

int enable_sata_clock(void)
{
	u32 reg;
	s32 timeout = 100000;

	/* Enable sata clock */
	reg = readl(&imx_ccm->CCGR5); /* CCGR5 */
	reg |= MXC_CCM_CCGR5_SATA_MASK;
	writel(reg, &imx_ccm->CCGR5);

	/* Enable PLLs */
	reg = readl(&anatop->pll_enet);
	reg &= ~BM_ANADIG_PLL_ENET_POWERDOWN;
	writel(reg, &anatop->pll_enet);
	reg |= BM_ANADIG_PLL_ENET_ENABLE;
	while (timeout--) {
		if (readl(&anatop->pll_enet) & BM_ANADIG_PLL_ENET_LOCK)
			break;
	}
	if (timeout <= 0)
		return -EIO;
	reg &= ~BM_ANADIG_PLL_ENET_BYPASS;
	writel(reg, &anatop->pll_enet);
	reg |= BM_ANADIG_PLL_ENET_ENABLE_SATA;
	writel(reg, &anatop->pll_enet);

	return 0 ;
}

void ipu_clk_enable(void)
{
	u32 reg = readl(&imx_ccm->CCGR3);
	reg |= MXC_CCM_CCGR3_IPU1_IPU_MASK;
	writel(reg, &imx_ccm->CCGR3);
}

void ipu_clk_disable(void)
{
	u32 reg = readl(&imx_ccm->CCGR3);
	reg &= ~MXC_CCM_CCGR3_IPU1_IPU_MASK;
	writel(reg, &imx_ccm->CCGR3);
}

void ipu_di_clk_enable(int di)
{
	switch (di) {
	case 0:
		setbits_le32(&imx_ccm->CCGR3,
			MXC_CCM_CCGR3_IPU1_IPU_DI0_MASK);
		break;
	case 1:
		setbits_le32(&imx_ccm->CCGR3,
			MXC_CCM_CCGR3_IPU1_IPU_DI1_MASK);
		break;
	default:
		printf("%s: Invalid DI index %d\n", __func__, di);
	}
}

void ipu_di_clk_disable(int di)
{
	switch (di) {
	case 0:
		clrbits_le32(&imx_ccm->CCGR3,
			MXC_CCM_CCGR3_IPU1_IPU_DI0_MASK);
		break;
	case 1:
		clrbits_le32(&imx_ccm->CCGR3,
			MXC_CCM_CCGR3_IPU1_IPU_DI1_MASK);
		break;
	default:
		printf("%s: Invalid DI index %d\n", __func__, di);
	}
}

void ldb_clk_enable(int ldb)
{
	switch (ldb) {
	case 0:
		setbits_le32(&imx_ccm->CCGR3,
			MXC_CCM_CCGR3_LDB_DI0_MASK);
		break;
	case 1:
		setbits_le32(&imx_ccm->CCGR3,
			MXC_CCM_CCGR3_LDB_DI1_MASK);
		break;
	default:
		printf("%s: Invalid LDB index %d\n", __func__, ldb);
	}
}

void ldb_clk_disable(int ldb)
{
	switch (ldb) {
	case 0:
		clrbits_le32(&imx_ccm->CCGR3,
			MXC_CCM_CCGR3_LDB_DI0_MASK);
		break;
	case 1:
		clrbits_le32(&imx_ccm->CCGR3,
			MXC_CCM_CCGR3_LDB_DI1_MASK);
		break;
	default:
		printf("%s: Invalid LDB index %d\n", __func__, ldb);
	}
}

void ocotp_clk_enable(void)
{
	u32 reg = readl(&imx_ccm->CCGR2);
	reg |= MXC_CCM_CCGR2_OCOTP_CTRL_MASK;
	writel(reg, &imx_ccm->CCGR2);
}

void ocotp_clk_disable(void)
{
	u32 reg = readl(&imx_ccm->CCGR2);
	reg &= ~MXC_CCM_CCGR2_OCOTP_CTRL_MASK;
	writel(reg, &imx_ccm->CCGR2);
}

unsigned int mxc_get_clock(enum mxc_clock clk)
{
	switch (clk) {
	case MXC_ARM_CLK:
		return get_mcu_main_clk();
	case MXC_PER_CLK:
		return get_periph_clk();
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
		return get_cspi_clk();
	case MXC_AXI_CLK:
		return get_axi_clk();
	case MXC_EMI_SLOW_CLK:
		return get_emi_slow_clk();
	case MXC_DDR_CLK:
		return get_mmdc_ch0_clk();
	case MXC_ESDHC_CLK:
		return get_usdhc_clk(0);
	case MXC_ESDHC2_CLK:
		return get_usdhc_clk(1);
	case MXC_ESDHC3_CLK:
		return get_usdhc_clk(2);
	case MXC_ESDHC4_CLK:
		return get_usdhc_clk(3);
	case MXC_SATA_CLK:
		return get_ahb_clk();
	case MXC_NFC_CLK:
		return get_nfc_clk();
	}

	return -1;
}

static inline int gcd(int m, int n)
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

/* Config CPU clock */
static int set_arm_clk(u32 ref, u32 freq_khz)
{
	int d;
	int div = 0;
	int mul = 0;
	u32 min_err = ~0;
	u32 reg;

	if (freq_khz > ref / 1000 * 108 / 2 || freq_khz < ref / 1000 * 54 / 8 / 2) {
		printf("Frequency %u.%03uMHz is out of range: %u.%03u..%u.%03u\n",
			freq_khz / 1000, freq_khz % 1000,
			54 * ref / 1000000 / 8 / 2, 54 * ref / 1000 / 8 / 2 % 1000,
			108 * ref / 1000000 / 2, 108 * ref / 1000 / 2 % 1000);
		return -EINVAL;
	}

	for (d = DIV_ROUND_UP(648000, freq_khz); d <= 8; d++) {
		int m = freq_khz * 2 * d / (ref / 1000);
		u32 f;
		u32 err;

		if (m > 108) {
			debug("%s@%d: d=%d m=%d\n", __func__, __LINE__,
				d, m);
			break;
		}

		f = ref * m / d / 2;
		if (f > freq_khz * 1000) {
			debug("%s@%d: d=%d m=%d f=%u freq=%u\n", __func__, __LINE__,
				d, m, f, freq_khz);
			if (--m < 54)
				return -EINVAL;
			f = ref * m / d / 2;
		}
		err = freq_khz * 1000 - f;
		debug("%s@%d: d=%d m=%d f=%u freq=%u err=%d\n", __func__, __LINE__,
			d, m, f, freq_khz, err);
		if (err < min_err) {
			mul = m;
			div = d;
			min_err = err;
			if (err == 0)
				break;
		}
	}
	if (min_err == ~0)
		return -EINVAL;
	debug("Setting M=%3u D=%2u for %u.%03uMHz (actual: %u.%03uMHz)\n",
		mul, div, freq_khz / 1000, freq_khz % 1000,
		ref * mul / 2 / div / 1000000, ref * mul / 2 / div / 1000 % 1000);

	reg = readl(&anatop->pll_arm);
	debug("anadig_pll_arm=%08x -> %08x\n",
		reg, (reg & ~0x7f) | mul);

	reg |= 1 << 16;
	writel(reg, &anatop->pll_arm); /* bypass PLL */

	reg = (reg & ~0x7f) | mul;
	writel(reg, &anatop->pll_arm);

	writel(div - 1, &imx_ccm->cacrr);

	reg &= ~(1 << 16);
	writel(reg, &anatop->pll_arm); /* disable PLL bypass */

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
	int ret;

	freq *= 1000;

	switch (clk) {
	case MXC_ARM_CLK:
		ret = set_arm_clk(ref, freq);
		break;

	case MXC_NFC_CLK:
		ret = set_nfc_clk(ref, freq);
		break;

	default:
		printf("Warning: Unsupported or invalid clock type: %d\n",
			clk);
		return -EINVAL;
	}

	return ret;
}

/*
 * Dump some core clocks.
 */
#define print_pll(pll)	{				\
	u32 __pll = decode_pll(pll, MXC_HCLK);		\
	printf("%-12s %4d.%03d MHz\n", #pll,		\
		__pll / 1000000, __pll / 1000 % 1000);	\
	}

#define MXC_IPG_PER_CLK	MXC_IPG_PERCLK

#define print_clk(clk)	{				\
	u32 __clk = mxc_get_clock(MXC_##clk##_CLK);	\
	printf("%-12s %4d.%03d MHz\n", #clk,		\
		__clk / 1000000, __clk / 1000 % 1000);	\
	}

#define print_pfd(pll, pfd)	{					\
	u32 __pfd = readl(&anatop->pfd_##pll);				\
	if (__pfd & (0x80 << 8 * pfd)) {				\
		printf("PFD_%s[%d]      OFF\n", #pll, pfd);		\
	} else {							\
		__pfd = (__pfd >> 8 * pfd) & 0x3f;			\
		printf("PFD_%s[%d]   %4d.%03d MHz\n", #pll, pfd,	\
			pll * 18 / __pfd,				\
			pll * 18 * 1000 / __pfd % 1000);		\
	}								\
}

static void do_mx6_showclocks(void)
{
	print_pll(PLL_ARM);
	print_pll(PLL_BUS);
	print_pll(PLL_USBOTG);
	print_pll(PLL_AUDIO);
	print_pll(PLL_VIDEO);
	print_pll(PLL_ENET);
	print_pll(PLL_USB2);
	printf("\n");

	print_pfd(480, 0);
	print_pfd(480, 1);
	print_pfd(480, 2);
	print_pfd(480, 3);
	print_pfd(528, 0);
	print_pfd(528, 1);
	print_pfd(528, 2);
	print_pfd(528, 3);
	printf("\n");

	print_clk(IPG);
	print_clk(UART);
	print_clk(CSPI);
	print_clk(AHB);
	print_clk(AXI);
	print_clk(DDR);
	print_clk(ESDHC);
	print_clk(ESDHC2);
	print_clk(ESDHC3);
	print_clk(ESDHC4);
	print_clk(EMI_SLOW);
	print_clk(NFC);
	print_clk(IPG_PER);
	print_clk(ARM);
}

static struct clk_lookup {
	const char *name;
	unsigned int index;
} mx6_clk_lookup[] = {
	{ "arm", MXC_ARM_CLK, },
	{ "nfc", MXC_NFC_CLK, },
};

int do_clocks(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int i;
	unsigned long freq;
	unsigned long ref = ~0UL;

	if (argc < 2) {
		do_mx6_showclocks();
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
	for (i = 0; i < ARRAY_SIZE(mx6_clk_lookup); i++) {
		if (strcasecmp(argv[1], mx6_clk_lookup[i].name) == 0) {
			switch (mx6_clk_lookup[i].index) {
			case MXC_ARM_CLK:
				if (argc > 3)
					return CMD_RET_USAGE;
				ref = CONFIG_SYS_MX6_HCLK;
				break;

			case MXC_NFC_CLK:
				if (argc > 3 && ref > 3) {
					printf("Invalid clock selector value: %lu\n", ref);
					return CMD_RET_FAILURE;
				}
				break;
			}
			printf("Setting %s clock to %lu MHz\n",
				mx6_clk_lookup[i].name, freq);
			if (mxc_set_clock(ref, freq, mx6_clk_lookup[i].index))
				break;
			freq = mxc_get_clock(mx6_clk_lookup[i].index);
			printf("%s clock set to %lu.%03lu MHz\n",
				mx6_clk_lookup[i].name,
				freq / 1000000, freq / 1000 % 1000);
			return CMD_RET_SUCCESS;
		}
	}
	if (i == ARRAY_SIZE(mx6_clk_lookup)) {
		printf("clock %s not found; supported clocks are:\n", argv[1]);
		for (i = 0; i < ARRAY_SIZE(mx6_clk_lookup); i++) {
			printf("\t%s\n", mx6_clk_lookup[i].name);
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
