/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <div64.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>

enum pll_clocks {
	PLL_ARM,	/* PLL1: ARM PLL */
	PLL_528,	/* PLL2: System Bus PLL*/
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

#define PLL_LOCK_BIT		(1 << 31)

static inline int wait_pll_lock(u32 *reg)
{
	int loops = 0;
	u32 val;

	while (!((val = readl(reg)) & PLL_LOCK_BIT)) {
		loops++;
		if (loops > 1000)
			break;
		udelay(1);
	}
	if (!(val & PLL_LOCK_BIT) && !(readl(reg) & PLL_LOCK_BIT))
		return -ETIMEDOUT;
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

#ifdef CONFIG_NAND_MXS
void setup_gpmi_io_clk(u32 cfg)
{
	/* Disable clocks per ERR007177 from MX6 errata */
	clrbits_le32(&imx_ccm->CCGR4,
		     MXC_CCM_CCGR4_RAWNAND_U_BCH_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_BCH_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_PL301_MX6QPER1_BCH_MASK);

	clrbits_le32(&imx_ccm->CCGR2, MXC_CCM_CCGR2_IOMUX_IPT_CLK_IO_MASK);

	clrsetbits_le32(&imx_ccm->cs2cdr,
			MXC_CCM_CS2CDR_ENFC_CLK_PODF_MASK |
			MXC_CCM_CS2CDR_ENFC_CLK_PRED_MASK |
			MXC_CCM_CS2CDR_ENFC_CLK_SEL_MASK,
			cfg);

	setbits_le32(&imx_ccm->CCGR2, MXC_CCM_CCGR2_IOMUX_IPT_CLK_IO_MASK);
	setbits_le32(&imx_ccm->CCGR4,
		     MXC_CCM_CCGR4_RAWNAND_U_BCH_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_BCH_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_PL301_MX6QPER1_BCH_MASK);
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

#if defined(CONFIG_FEC_MXC) && !defined(CONFIG_SOC_MX6SX)
void enable_enet_clk(unsigned char enable)
{
	u32 mask, *addr;

	if (is_cpu_type(MXC_CPU_MX6UL)) {
		mask = MXC_CCM_CCGR3_ENET_MASK;
		addr = &imx_ccm->CCGR3;
	} else {
		mask = MXC_CCM_CCGR1_ENET_MASK;
		addr = &imx_ccm->CCGR1;
	}

	if (enable)
		setbits_le32(addr, mask);
	else
		clrbits_le32(addr, mask);
}
#endif

#ifdef CONFIG_MXC_UART
void enable_uart_clk(unsigned char enable)
{
	u32 mask;

	if (is_cpu_type(MXC_CPU_MX6UL))
		mask = MXC_CCM_CCGR5_UART_MASK;
	else
		mask = MXC_CCM_CCGR5_UART_MASK | MXC_CCM_CCGR5_UART_SERIAL_MASK;

	if (enable)
		setbits_le32(&imx_ccm->CCGR5, mask);
	else
		clrbits_le32(&imx_ccm->CCGR5, mask);
}
#endif

#ifdef CONFIG_MMC
int enable_usdhc_clk(unsigned char enable, unsigned bus_num)
{
	u32 mask;

	if (bus_num > 3)
		return -EINVAL;

	mask = MXC_CCM_CCGR_CG_MASK << (bus_num * 2 + 2);
	if (enable)
		setbits_le32(&imx_ccm->CCGR6, mask);
	else
		clrbits_le32(&imx_ccm->CCGR6, mask);

	return 0;
}
#endif

#ifdef CONFIG_SYS_I2C_MXC
/* i2c_num can be from 0 - 3 */
int enable_i2c_clk(unsigned char enable, unsigned i2c_num)
{
	u32 reg;
	u32 mask;
	u32 *addr;

	if (i2c_num > 3)
		return -EINVAL;
	if (i2c_num < 3) {
		mask = MXC_CCM_CCGR_CG_MASK
			<< (MXC_CCM_CCGR2_I2C1_SERIAL_OFFSET
			+ (i2c_num << 1));
		reg = __raw_readl(&imx_ccm->CCGR2);
		if (enable)
			reg |= mask;
		else
			reg &= ~mask;
		__raw_writel(reg, &imx_ccm->CCGR2);
	} else {
		if (is_cpu_type(MXC_CPU_MX6SX) || is_cpu_type(MXC_CPU_MX6UL)) {
			mask = MXC_CCM_CCGR6_I2C4_MASK;
			addr = &imx_ccm->CCGR6;
		} else {
			mask = MXC_CCM_CCGR1_I2C4_SERIAL_MASK;
			addr = &imx_ccm->CCGR1;
		}
		reg = __raw_readl(addr);
		if (enable)
			reg |= mask;
		else
			reg &= ~mask;
		__raw_writel(reg, addr);
	}
	return 0;
}
#endif

/* spi_num can be from 0 - SPI_MAX_NUM */
int enable_spi_clk(unsigned char enable, unsigned spi_num)
{
	u32 reg;
	u32 mask;

	if (spi_num > SPI_MAX_NUM)
		return -EINVAL;

	mask = MXC_CCM_CCGR_CG_MASK << (spi_num << 1);
	reg = __raw_readl(&imx_ccm->CCGR1);
	if (enable)
		reg |= mask;
	else
		reg &= ~mask;
	__raw_writel(reg, &imx_ccm->CCGR1);
	return 0;
}

static u32 decode_pll(enum pll_clocks pll, u32 infreq)
{
	u32 div, post_div;

	switch (pll) {
	case PLL_ARM:
		div = __raw_readl(&anatop->pll_arm);
		if (div & BM_ANADIG_PLL_ARM_BYPASS)
			/* Assume the bypass clock is always derived from OSC */
			return infreq;
		div &= BM_ANADIG_PLL_ARM_DIV_SELECT;

		return infreq * div / 2;
	case PLL_528:
		div = __raw_readl(&anatop->pll_528);
		if (div & BM_ANADIG_PLL_528_BYPASS)
			return infreq;
		div &= BM_ANADIG_PLL_528_DIV_SELECT;

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
		post_div = (div & BM_ANADIG_PLL_AUDIO_POST_DIV_SELECT) >>
			BP_ANADIG_PLL_AUDIO_POST_DIV_SELECT;
		post_div = 1 << (2 - post_div);
		div &= BM_ANADIG_PLL_AUDIO_DIV_SELECT;

		return lldiv((u64)infreq * div, post_div);
	case PLL_VIDEO:
		div = __raw_readl(&anatop->pll_video);
		if (div & BM_ANADIG_PLL_VIDEO_BYPASS)
			return infreq;
		post_div = (div & BM_ANADIG_PLL_VIDEO_POST_DIV_SELECT) >>
			BP_ANADIG_PLL_VIDEO_POST_DIV_SELECT;
		post_div = 1 << (2 - post_div);
		div &= BM_ANADIG_PLL_VIDEO_DIV_SELECT;

		return lldiv((u64)infreq * div, post_div);
	case PLL_ENET:
		div = __raw_readl(&anatop->pll_enet);
		if (div & BM_ANADIG_PLL_ENET_BYPASS)
			return infreq;
		div &= BM_ANADIG_PLL_ENET_DIV_SELECT;

		return 25000000 * (div + (div >> 1) + 1);
	case PLL_USB2:
		div = __raw_readl(&anatop->usb2_pll_480_ctrl);
		if (div & BM_ANADIG_USB1_PLL_480_CTRL_BYPASS)
			return infreq;
		div &= BM_ANADIG_USB1_PLL_480_CTRL_DIV_SELECT;

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

static u32 mxc_get_pll_pfd(enum pll_clocks pll, int pfd_num)
{
	u32 div;
	u64 freq;

	switch (pll) {
	case PLL_528:
		if (!is_cpu_type(MXC_CPU_MX6UL)) {
			if (pfd_num == 3) {
				/* No PFD3 on PPL2 */
				return 0;
			}
		}
		div = __raw_readl(&anatop->pfd_528);
		freq = (u64)decode_pll(PLL_528, MXC_HCLK);
		break;
	case PLL_USBOTG:
		div = __raw_readl(&anatop->pfd_480);
		freq = (u64)decode_pll(PLL_USBOTG, MXC_HCLK);
		break;
	default:
		/* No PFD on other PLL */
		return 0;
	}

	return lldiv(freq * 18, (div & ANATOP_PFD_FRAC_MASK(pfd_num)) >>
			      ANATOP_PFD_FRAC_SHIFT(pfd_num));
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
	u32 reg, div = 0, freq = 0;

	reg = __raw_readl(&imx_ccm->cbcdr);
	if (reg & MXC_CCM_CBCDR_PERIPH_CLK_SEL) {
		div = (reg & MXC_CCM_CBCDR_PERIPH_CLK2_PODF_MASK) >>
		       MXC_CCM_CBCDR_PERIPH_CLK2_PODF_OFFSET;
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
			freq = decode_pll(PLL_528, MXC_HCLK);
			break;
		case 1:
			freq = mxc_get_pll_pfd(PLL_528, 2);
			break;
		case 2:
			freq = mxc_get_pll_pfd(PLL_528, 0);
			break;
		case 3:
			/* static / 2 divider */
			freq = mxc_get_pll_pfd(PLL_528, 2) / 2;
			break;
		}
	}

	return freq / (div + 1);
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
	if (is_cpu_type(MXC_CPU_MX6SL) || is_cpu_type(MXC_CPU_MX6SX) ||
	    is_mx6dqp() || is_cpu_type(MXC_CPU_MX6UL)) {
		if (reg & MXC_CCM_CSCMR1_PER_CLK_SEL_MASK)
			return MXC_HCLK; /* OSC 24Mhz */
	}

	perclk_podf = reg & MXC_CCM_CSCMR1_PERCLK_PODF_MASK;

	return get_ipg_clk() / (perclk_podf + 1);
}

static u32 get_uart_clk(void)
{
	u32 reg, uart_podf;
	u32 freq = decode_pll(PLL_USBOTG, MXC_HCLK) / 6; /* static divider */
	reg = __raw_readl(&imx_ccm->cscdr1);

	if (is_cpu_type(MXC_CPU_MX6SL) || is_cpu_type(MXC_CPU_MX6SX) ||
	    is_mx6dqp() || is_cpu_type(MXC_CPU_MX6UL)) {
		if (reg & MXC_CCM_CSCDR1_UART_CLK_SEL)
			freq = MXC_HCLK;
	}

	reg &= MXC_CCM_CSCDR1_UART_CLK_PODF_MASK;
	uart_podf = reg >> MXC_CCM_CSCDR1_UART_CLK_PODF_OFFSET;

	return freq / (uart_podf + 1);
}

static u32 get_cspi_clk(void)
{
	u32 reg, cspi_podf;

	reg = __raw_readl(&imx_ccm->cscdr2);
	cspi_podf = (reg & MXC_CCM_CSCDR2_ECSPI_CLK_PODF_MASK) >>
		     MXC_CCM_CSCDR2_ECSPI_CLK_PODF_OFFSET;

	if (is_mx6dqp() || is_cpu_type(MXC_CPU_MX6SL) ||
	    is_cpu_type(MXC_CPU_MX6SX) || is_cpu_type(MXC_CPU_MX6UL)) {
		if (reg & MXC_CCM_CSCDR2_ECSPI_CLK_SEL_MASK)
			return MXC_HCLK / (cspi_podf + 1);
	}

	return	decode_pll(PLL_USBOTG, MXC_HCLK) / (8 * (cspi_podf + 1));
}

static u32 get_axi_clk(void)
{
	u32 root_freq, axi_podf;
	u32 cbcdr = __raw_readl(&imx_ccm->cbcdr);

	axi_podf = cbcdr & MXC_CCM_CBCDR_AXI_PODF_MASK;
	axi_podf >>= MXC_CCM_CBCDR_AXI_PODF_OFFSET;

	if (cbcdr & MXC_CCM_CBCDR_AXI_SEL) {
		if (cbcdr & MXC_CCM_CBCDR_AXI_ALT_SEL)
			root_freq = mxc_get_pll_pfd(PLL_528, 2);
		else
			root_freq = mxc_get_pll_pfd(PLL_USBOTG, 1);
	} else {
		root_freq = get_periph_clk();
	}
	return  root_freq / (axi_podf + 1);
}

static u32 get_emi_slow_clk(void)
{
	u32 emi_clk_sel, emi_slow_podf, cscmr1, root_freq = 0;

	cscmr1 =  __raw_readl(&imx_ccm->cscmr1);
	emi_clk_sel = cscmr1 & MXC_CCM_CSCMR1_ACLK_EMI_SLOW_MASK;
	emi_clk_sel >>= MXC_CCM_CSCMR1_ACLK_EMI_SLOW_OFFSET;
	emi_slow_podf = cscmr1 & MXC_CCM_CSCMR1_ACLK_EMI_SLOW_PODF_MASK;
	emi_slow_podf >>= MXC_CCM_CSCMR1_ACLK_EMI_SLOW_PODF_OFFSET;

	switch (emi_clk_sel) {
	case 0:
		root_freq = get_axi_clk();
		break;
	case 1:
		root_freq = decode_pll(PLL_USBOTG, MXC_HCLK);
		break;
	case 2:
		root_freq =  mxc_get_pll_pfd(PLL_528, 2);
		break;
	case 3:
		root_freq =  mxc_get_pll_pfd(PLL_528, 0);
		break;
	}

	return root_freq / (emi_slow_podf + 1);
}

static u32 get_nfc_clk(void)
{
	u32 cs2cdr = __raw_readl(&imx_ccm->cs2cdr);
	u32 podf = (cs2cdr & MXC_CCM_CS2CDR_ENFC_CLK_PODF_MASK) >>
		MXC_CCM_CS2CDR_ENFC_CLK_PODF_OFFSET;
	u32 pred = (cs2cdr & MXC_CCM_CS2CDR_ENFC_CLK_PRED_MASK) >>
		MXC_CCM_CS2CDR_ENFC_CLK_PRED_OFFSET;
	int nfc_clk_sel = (cs2cdr & MXC_CCM_CS2CDR_ENFC_CLK_SEL_MASK) >>
		MXC_CCM_CS2CDR_ENFC_CLK_SEL_OFFSET;
	u32 root_freq;

	switch (nfc_clk_sel) {
	case 0:
		root_freq = mxc_get_pll_pfd(PLL_528, 0);
		break;
	case 1:
		root_freq = decode_pll(PLL_528, MXC_HCLK);
		break;
	case 2:
		root_freq = decode_pll(PLL_USBOTG, MXC_HCLK);
		break;
	case 3:
		root_freq = mxc_get_pll_pfd(PLL_528, 2);
		break;
	case 4:
		root_freq = mxc_get_pll_pfd(PLL_USBOTG, 3);
		break;
	default:
		return 0;
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
			root_freq = mxc_get_pll_pfd(PLL_528, 0);
			break;
		case 1:
			root_freq = decode_pll(PLL_528, MXC_HCLK);
			break;
		case 2:
			root_freq = decode_pll(PLL_USBOTG, MXC_HCLK);
			break;
		case 3:
			root_freq = mxc_get_pll_pfd(PLL_528, 2);
			break;
		}
		if (root_freq < freq)
			continue;

		podf = min(DIV_ROUND_UP(root_freq, freq), 1U << 6);
		pred = min(DIV_ROUND_UP(root_freq / podf, freq), 8U);
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

static u32 get_mmdc_ch0_clk(void)
{
	u32 cbcmr = __raw_readl(&imx_ccm->cbcmr);
	u32 cbcdr = __raw_readl(&imx_ccm->cbcdr);

	u32 freq, podf, per2_clk2_podf;

	if (is_cpu_type(MXC_CPU_MX6SX) || is_cpu_type(MXC_CPU_MX6UL) ||
	    is_cpu_type(MXC_CPU_MX6SL)) {
		podf = (cbcdr & MXC_CCM_CBCDR_MMDC_CH1_PODF_MASK) >>
			MXC_CCM_CBCDR_MMDC_CH1_PODF_OFFSET;
		if (cbcdr & MXC_CCM_CBCDR_PERIPH2_CLK_SEL) {
			per2_clk2_podf = (cbcdr & MXC_CCM_CBCDR_PERIPH2_CLK2_PODF_MASK) >>
				MXC_CCM_CBCDR_PERIPH2_CLK2_PODF_OFFSET;
			if (is_cpu_type(MXC_CPU_MX6SL)) {
				if (cbcmr & MXC_CCM_CBCMR_PERIPH2_CLK2_SEL)
					freq = MXC_HCLK;
				else
					freq = decode_pll(PLL_USBOTG, MXC_HCLK);
			} else {
				if (cbcmr & MXC_CCM_CBCMR_PERIPH2_CLK2_SEL)
					freq = decode_pll(PLL_528, MXC_HCLK);
				else
					freq = decode_pll(PLL_USBOTG, MXC_HCLK);
			}
		} else {
			per2_clk2_podf = 0;
			switch ((cbcmr &
				MXC_CCM_CBCMR_PRE_PERIPH2_CLK_SEL_MASK) >>
				MXC_CCM_CBCMR_PRE_PERIPH2_CLK_SEL_OFFSET) {
			case 0:
				freq = decode_pll(PLL_528, MXC_HCLK);
				break;
			case 1:
				freq = mxc_get_pll_pfd(PLL_528, 2);
				break;
			case 2:
				freq = mxc_get_pll_pfd(PLL_528, 0);
				break;
			case 3:
				/* static / 2 divider */
				freq =  mxc_get_pll_pfd(PLL_528, 2) / 2;
				break;
			}
		}
		return freq / (podf + 1) / (per2_clk2_podf + 1);
	} else {
		podf = (cbcdr & MXC_CCM_CBCDR_MMDC_CH0_PODF_MASK) >>
			MXC_CCM_CBCDR_MMDC_CH0_PODF_OFFSET;
		return get_periph_clk() / (podf + 1);
	}
}

#ifdef CONFIG_FSL_QSPI
/* qspi_num can be from 0 - 1 */
void enable_qspi_clk(int qspi_num)
{
	u32 reg = 0;
	/* Enable QuadSPI clock */
	switch (qspi_num) {
	case 0:
		/* disable the clock gate */
		clrbits_le32(&imx_ccm->CCGR3, MXC_CCM_CCGR3_QSPI1_MASK);

		/* set 50M  : (50 = 396 / 2 / 4) */
		reg = readl(&imx_ccm->cscmr1);
		reg &= ~(MXC_CCM_CSCMR1_QSPI1_PODF_MASK |
			 MXC_CCM_CSCMR1_QSPI1_CLK_SEL_MASK);
		reg |= ((1 << MXC_CCM_CSCMR1_QSPI1_PODF_OFFSET) |
			(2 << MXC_CCM_CSCMR1_QSPI1_CLK_SEL_OFFSET));
		writel(reg, &imx_ccm->cscmr1);

		/* enable the clock gate */
		setbits_le32(&imx_ccm->CCGR3, MXC_CCM_CCGR3_QSPI1_MASK);
		break;
	case 1:
		/*
		 * disable the clock gate
		 * QSPI2 and GPMI_BCH_INPUT_GPMI_IO share the same clock gate,
		 * disable both of them.
		 */
		clrbits_le32(&imx_ccm->CCGR4, MXC_CCM_CCGR4_QSPI2_ENFC_MASK |
			     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK);

		/* set 50M  : (50 = 396 / 2 / 4) */
		reg = readl(&imx_ccm->cs2cdr);
		reg &= ~(MXC_CCM_CS2CDR_QSPI2_CLK_PODF_MASK |
			 MXC_CCM_CS2CDR_QSPI2_CLK_PRED_MASK |
			 MXC_CCM_CS2CDR_QSPI2_CLK_SEL_MASK);
		reg |= (MXC_CCM_CS2CDR_QSPI2_CLK_PRED(0x1) |
			MXC_CCM_CS2CDR_QSPI2_CLK_SEL(0x3));
		writel(reg, &imx_ccm->cs2cdr);

		/*enable the clock gate*/
		setbits_le32(&imx_ccm->CCGR4, MXC_CCM_CCGR4_QSPI2_ENFC_MASK |
			     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK);
		break;
	default:
		break;
	}
}
#endif

#ifdef CONFIG_FEC_MXC
int enable_fec_anatop_clock(enum enet_freq freq)
{
	u32 reg = 0;
	s32 timeout = 100000;

	if (freq < ENET_25MHZ || freq > ENET_125MHZ)
		return -EINVAL;

	reg = readl(&anatop->pll_enet);
	reg &= ~BM_ANADIG_PLL_ENET_DIV_SELECT;
	reg |= freq;

	if ((reg & BM_ANADIG_PLL_ENET_POWERDOWN) ||
	    (!(reg & BM_ANADIG_PLL_ENET_LOCK))) {
		reg &= ~BM_ANADIG_PLL_ENET_POWERDOWN;
		writel(reg, &anatop->pll_enet);
		while (timeout--) {
			if (readl(&anatop->pll_enet) & BM_ANADIG_PLL_ENET_LOCK)
				break;
		}
		if (timeout < 0)
			return -ETIMEDOUT;
	}

	/* Enable FEC clock */
	reg |= BM_ANADIG_PLL_ENET_ENABLE;
	reg &= ~BM_ANADIG_PLL_ENET_BYPASS;
	writel(reg, &anatop->pll_enet);

#ifdef CONFIG_SOC_MX6SX
	/*
	 * Set enet ahb clock to 200MHz
	 * pll2_pfd2_396m-> ENET_PODF-> ENET_AHB
	 */
	reg = readl(&imx_ccm->chsccdr);
	reg &= ~(MXC_CCM_CHSCCDR_ENET_PRE_CLK_SEL_MASK
		 | MXC_CCM_CHSCCDR_ENET_PODF_MASK
		 | MXC_CCM_CHSCCDR_ENET_CLK_SEL_MASK);
	/* PLL2 PFD2 */
	reg |= (4 << MXC_CCM_CHSCCDR_ENET_PRE_CLK_SEL_OFFSET);
	/* Div = 2*/
	reg |= (1 << MXC_CCM_CHSCCDR_ENET_PODF_OFFSET);
	reg |= (0 << MXC_CCM_CHSCCDR_ENET_CLK_SEL_OFFSET);
	writel(reg, &imx_ccm->chsccdr);

	/* Enable enet system clock */
	reg = readl(&imx_ccm->CCGR3);
	reg |= MXC_CCM_CCGR3_ENET_MASK;
	writel(reg, &imx_ccm->CCGR3);
#endif
	return 0;
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
		root_freq = mxc_get_pll_pfd(PLL_528, 0);
	else
		root_freq = mxc_get_pll_pfd(PLL_528, 2);

	return root_freq / (usdhc_podf + 1);
}

u32 imx_get_uartclk(void)
{
	return get_uart_clk();
}

u32 imx_get_fecclk(void)
{
	return mxc_get_clock(MXC_IPG_CLK);
}

#if defined(CONFIG_CMD_SATA) || defined(CONFIG_PCIE_IMX)
static int enable_enet_pll(uint32_t en)
{
	u32 reg;
	s32 timeout = 100000;

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
	reg |= en;
	writel(reg, &anatop->pll_enet);
	return 0;
}
#endif

#ifdef CONFIG_CMD_SATA
static void ungate_sata_clock(void)
{
	/* Enable SATA clock. */
	setbits_le32(&imx_ccm->CCGR5, MXC_CCM_CCGR5_SATA_MASK);
}

int enable_sata_clock(void)
{
	ungate_sata_clock();
	return enable_enet_pll(BM_ANADIG_PLL_ENET_ENABLE_SATA);
}

void disable_sata_clock(void)
{
	clrbits_le32(&imx_ccm->CCGR5, MXC_CCM_CCGR5_SATA_MASK);
}
#endif

#ifdef CONFIG_PCIE_IMX
static void ungate_pcie_clock(void)
{
	/* Enable PCIe clock. */
	setbits_le32(&imx_ccm->CCGR4, MXC_CCM_CCGR4_PCIE_MASK);
}

int enable_pcie_clock(void)
{
	u32 lvds1_clk_sel;

	/*
	 * Here be dragons!
	 *
	 * The register ANATOP_MISC1 is not documented in the Freescale
	 * MX6RM. The register that is mapped in the ANATOP space and
	 * marked as ANATOP_MISC1 is actually documented in the PMU section
	 * of the datasheet as PMU_MISC1.
	 *
	 * Switch LVDS clock source to SATA (0xb) on mx6q/dl or PCI (0xa) on
	 * mx6sx, disable clock INPUT and enable clock OUTPUT. This is important
	 * for PCI express link that is clocked from the i.MX6.
	 */
#define ANADIG_ANA_MISC1_LVDSCLK1_IBEN		(1 << 12)
#define ANADIG_ANA_MISC1_LVDSCLK1_OBEN		(1 << 10)
#define ANADIG_ANA_MISC1_LVDS1_CLK_SEL_MASK	0x0000001F
#define ANADIG_ANA_MISC1_LVDS1_CLK_SEL_PCIE_REF	0xa
#define ANADIG_ANA_MISC1_LVDS1_CLK_SEL_SATA_REF	0xb

	if (is_cpu_type(MXC_CPU_MX6SX))
		lvds1_clk_sel = ANADIG_ANA_MISC1_LVDS1_CLK_SEL_PCIE_REF;
	else
		lvds1_clk_sel = ANADIG_ANA_MISC1_LVDS1_CLK_SEL_SATA_REF;

	clrsetbits_le32(&anatop_regs->ana_misc1,
			ANADIG_ANA_MISC1_LVDSCLK1_IBEN |
			ANADIG_ANA_MISC1_LVDS1_CLK_SEL_MASK,
			ANADIG_ANA_MISC1_LVDSCLK1_OBEN | lvds1_clk_sel);

	/* PCIe reference clock sourced from AXI. */
	clrbits_le32(&ccm_regs->cbcmr, MXC_CCM_CBCMR_PCIE_AXI_CLK_SEL);

	/* Party time! Ungate the clock to the PCIe. */
#ifdef CONFIG_CMD_SATA
	ungate_sata_clock();
#endif
	ungate_pcie_clock();

	return enable_enet_pll(BM_ANADIG_PLL_ENET_ENABLE_SATA |
			       BM_ANADIG_PLL_ENET_ENABLE_PCIE);
}
#endif

#ifdef CONFIG_SECURE_BOOT
void hab_caam_clock_enable(unsigned char enable)
{
	u32 reg;

	/* CG4 ~ CG6, CAAM clocks */
	reg = __raw_readl(&imx_ccm->CCGR0);
	if (enable)
		reg |= (MXC_CCM_CCGR0_CAAM_WRAPPER_IPG_MASK |
			MXC_CCM_CCGR0_CAAM_WRAPPER_ACLK_MASK |
			MXC_CCM_CCGR0_CAAM_SECURE_MEM_MASK);
	else
		reg &= ~(MXC_CCM_CCGR0_CAAM_WRAPPER_IPG_MASK |
			MXC_CCM_CCGR0_CAAM_WRAPPER_ACLK_MASK |
			MXC_CCM_CCGR0_CAAM_SECURE_MEM_MASK);
	__raw_writel(reg, &imx_ccm->CCGR0);

	/* EMI slow clk */
	reg = __raw_readl(&imx_ccm->CCGR6);
	if (enable)
		reg |= MXC_CCM_CCGR6_EMI_SLOW_MASK;
	else
		reg &= ~MXC_CCM_CCGR6_EMI_SLOW_MASK;
	__raw_writel(reg, &imx_ccm->CCGR6);
}
#endif

static void enable_pll3(void)
{
	/* make sure pll3 is enabled */
	if ((readl(&anatop->usb1_pll_480_ctrl) &
			BM_ANADIG_USB1_PLL_480_CTRL_LOCK) == 0) {
		/* enable pll's power */
		writel(BM_ANADIG_USB1_PLL_480_CTRL_POWER,
		       &anatop->usb1_pll_480_ctrl_set);
		writel(0x80, &anatop->ana_misc2_clr);
		/* wait for pll lock */
		while ((readl(&anatop->usb1_pll_480_ctrl) &
			BM_ANADIG_USB1_PLL_480_CTRL_LOCK) == 0)
			;
		/* disable bypass */
		writel(BM_ANADIG_USB1_PLL_480_CTRL_BYPASS,
		       &anatop->usb1_pll_480_ctrl_clr);
		/* enable pll output */
		writel(BM_ANADIG_USB1_PLL_480_CTRL_ENABLE,
		       &anatop->usb1_pll_480_ctrl_set);
	}
}

void enable_thermal_clk(void)
{
	enable_pll3();
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

#ifdef CONFIG_VIDEO_MXS
void lcdif_clk_enable(void)
{
	setbits_le32(&imx_ccm->CCGR3, MXC_CCM_CCGR3_LCDIF_MASK);
	setbits_le32(&imx_ccm->CCGR2, MXC_CCM_CCGR2_LCD_MASK);
}

void lcdif_clk_disable(void)
{
	clrbits_le32(&imx_ccm->CCGR2, MXC_CCM_CCGR2_LCD_MASK);
	clrbits_le32(&imx_ccm->CCGR3, MXC_CCM_CCGR3_LCDIF_MASK);
}

#define CBCMR_LCDIF_MASK	MXC_CCM_CBCMR_LCDIF_PODF_MASK
#define CSCDR2_LCDIF_MASK	(MXC_CCM_CSCDR2_LCDIF_PRED_MASK |	\
				MXC_CCM_CSCDR2_LCDIF_CLK_SEL_MASK)

static u32 get_lcdif_root_clk(u32 cscdr2)
{
	int lcdif_pre_clk_sel = (cscdr2 & MXC_CCM_CSCDR2_LCDIF_PRE_CLK_SEL_MASK) >>
		MXC_CCM_CSCDR2_LCDIF_PRE_CLK_SEL_OFFSET;
	int lcdif_clk_sel = (cscdr2 & MXC_CCM_CSCDR2_LCDIF_CLK_SEL_MASK) >>
		MXC_CCM_CSCDR2_LCDIF_CLK_SEL_OFFSET;
	u32 root_freq;

	switch (lcdif_clk_sel) {
	case 0:
		switch (lcdif_pre_clk_sel) {
		case 0:
			root_freq = decode_pll(PLL_528, MXC_HCLK);
			break;
		case 1:
			root_freq = mxc_get_pll_pfd(PLL_USBOTG, 3);
			break;
		case 2:
			root_freq = decode_pll(PLL_VIDEO, MXC_HCLK);
			break;
		case 3:
			root_freq = mxc_get_pll_pfd(PLL_528, 0);
			break;
		case 4:
			root_freq = mxc_get_pll_pfd(PLL_528, 1);
			break;
		case 5:
			root_freq = mxc_get_pll_pfd(PLL_USBOTG, 1);
			break;
		default:
			return 0;
		}
		break;
	case 1:
		root_freq = mxc_get_pll_pfd(PLL_VIDEO, 0);
		break;
	case 2:
		root_freq = decode_pll(PLL_USBOTG, MXC_HCLK);
		break;
	case 3:
		root_freq = mxc_get_pll_pfd(PLL_528, 2);
		break;
	default:
		return 0;
	}

	return root_freq;
}

static int set_lcdif_pll(u32 ref, u32 freq_khz,
			unsigned post_div)
{
	int ret;
	u64 freq = freq_khz * 1000;
	u32 post_div_mask = 1 << (2 - post_div);
	int mul = 1;
	u32 min_err = ~0;
	u32 reg;
	int num = 0;
	int denom = 1;
	const int min_div = 27;
	const int max_div = 54;
	const int div_mask = 0x7f;
	const u32 max_freq = ref * max_div / post_div;
	const u32 min_freq = ref * min_div / post_div;

	if (freq > max_freq || freq < min_freq) {
		printf("Frequency %u.%03uMHz is out of range: %u.%03u..%u.%03uMHz\n",
			freq_khz / 1000, freq_khz % 1000,
			min_freq / 1000000, min_freq / 1000 % 1000,
			max_freq / 1000000, max_freq / 1000 % 1000);
		return -EINVAL;
	}
	{
		int d = post_div;
		int m = lldiv(freq * d + ref - 1, ref);
		u32 err;
		u32 f;

		debug("%s@%d: d=%d m=%d max_div=%u min_div=%u\n", __func__, __LINE__,
			d, m, max_div, min_div);
		if (m > max_div || m < min_div)
			return -EINVAL;

		f = ref * m / d;
		if (f > freq) {
			debug("%s@%d: d=%d m=%d f=%u freq=%llu\n", __func__, __LINE__,
				d, m, f, freq);
			return -EINVAL;
		}
		err = freq - f;
		debug("%s@%d: d=%d m=%d f=%u freq=%llu err=%d\n", __func__, __LINE__,
			d, m, f, freq, err);
		if (err < min_err) {
			mul = m;
			min_err = err;
		}
	}
	if (min_err == ~0) {
		printf("Cannot set VIDEO PLL to %u.%03uMHz\n",
			freq_khz / 1000, freq_khz % 1000);
		return -EINVAL;
	}

	debug("Setting M=%3u D=%u N=%d DE=%u for %u.%03uMHz (actual: %u.%03uMHz)\n",
		mul, post_div, num, denom,
		freq_khz / post_div / 1000, freq_khz / post_div % 1000,
		ref * mul / post_div / 1000000,
		ref * mul / post_div / 1000 % 1000);

	reg = readl(&anatop->pll_video);
	setbits_le32(&anatop->pll_video, BM_ANADIG_PLL_VIDEO_BYPASS);

	reg = (reg & ~(div_mask |
			BM_ANADIG_PLL_VIDEO_POST_DIV_SELECT)) |
		mul | (post_div_mask << BP_ANADIG_PLL_VIDEO_POST_DIV_SELECT);
	writel(reg, &anatop->pll_video);

	ret = wait_pll_lock(&anatop->pll_video);
	if (ret) {
		printf("Video PLL failed to lock\n");
		return ret;
	}

	clrbits_le32(&anatop->pll_video, BM_ANADIG_PLL_VIDEO_BYPASS);
	return 0;
}

static int set_lcdif_clk(u32 ref, u32 freq_khz)
{
	u32 cbcmr = __raw_readl(&imx_ccm->cbcmr);
	u32 cscdr2 = __raw_readl(&imx_ccm->cscdr2);
	u32 cbcmr_val;
	u32 cscdr2_val;
	u32 freq = freq_khz * 1000;
	u32 act_freq;
	u32 err;
	u32 min_div = 27;
	u32 max_div = 54;
	u32 min_pll_khz = ref * min_div / 4 / 1000;
	u32 max_pll_khz = ref * max_div / 1000;
	u32 pll_khz;
	u32 post_div = 0;
	u32 m;
	u32 min_err = ~0;
	u32 best_m = 0;
	u32 best_pred = 1;
	u32 best_podf = 1;
	u32 div;
	unsigned pd;

	if (freq_khz > max_pll_khz)
		return -EINVAL;

	for (pd = 1; min_err && pd <= 4; pd <<= 1) {
		for (m = max(min_div, DIV_ROUND_UP(648000 / pd, freq_khz * 64));
		     m <= max_div; m++) {
			u32 err;
			int pred = 0;
			int podf = 0;
			u32 root_freq = ref * m / pd;

			div = DIV_ROUND_UP(root_freq, freq);

			while (pred * podf == 0 && div <= 64) {
				int p1, p2;

				for (p1 = 1; p1 <= 8; p1++) {
					for (p2 = 1; p2 <= 8; p2++) {
						if (p1 * p2 == div) {
							podf = p1;
							pred = p2;
							break;
						}
					}
				}
				if (pred * podf == 0) {
					div++;
				}
			}
			if (pred * podf == 0)
				continue;

			/* relative error in per mille */
			act_freq = root_freq / div;
			err = abs(act_freq - freq) / freq_khz;

			if (err < min_err) {
				best_m = m;
				best_pred = pred;
				best_podf = podf;
				post_div = pd;
				min_err = err;
				if (err <= 10)
					break;
			}
		}
	}
	if (min_err > 50)
		return -EINVAL;

	pll_khz = ref / 1000 * best_m;
	if (pll_khz > max_pll_khz)
		return -EINVAL;

	if (pll_khz < min_pll_khz)
		return -EINVAL;

	err = set_lcdif_pll(ref, pll_khz / post_div, post_div);
	if (err)
		return err;

	cbcmr_val = (best_podf - 1) << MXC_CCM_CBCMR_LCDIF_PODF_OFFSET;
	cscdr2_val = (best_pred - 1) << MXC_CCM_CSCDR2_LCDIF_PRED_OFFSET;

	if ((cbcmr & CBCMR_LCDIF_MASK) != cbcmr_val) {
		debug("changing cbcmr from %08x to %08x\n", cbcmr,
			(cbcmr & ~CBCMR_LCDIF_MASK) | cbcmr_val);
		clrsetbits_le32(&imx_ccm->cbcmr,
				CBCMR_LCDIF_MASK,
				cbcmr_val);
	} else {
		debug("Leaving cbcmr unchanged [%08x]\n", cbcmr);
	}
	if ((cscdr2 & CSCDR2_LCDIF_MASK) != cscdr2_val) {
		debug("changing cscdr2 from %08x to %08x\n", cscdr2,
			(cscdr2 & ~CSCDR2_LCDIF_MASK) | cscdr2_val);
		clrsetbits_le32(&imx_ccm->cscdr2,
				CSCDR2_LCDIF_MASK,
				cscdr2_val);
	} else {
		debug("Leaving cscdr2 unchanged [%08x]\n", cscdr2);
	}
	return 0;
}

void mxs_set_lcdclk(u32 khz)
{
	set_lcdif_clk(CONFIG_SYS_MX6_HCLK, khz);
}

static u32 get_lcdif_clk(void)
{
	u32 cbcmr = __raw_readl(&imx_ccm->cbcmr);
	u32 podf = ((cbcmr & MXC_CCM_CBCMR_LCDIF_PODF_MASK) >>
		MXC_CCM_CBCMR_LCDIF_PODF_OFFSET) + 1;
	u32 cscdr2 = __raw_readl(&imx_ccm->cscdr2);
	u32 pred = ((cscdr2 & MXC_CCM_CSCDR2_LCDIF_PRED_MASK) >>
		MXC_CCM_CSCDR2_LCDIF_PRED_OFFSET) + 1;
	u32 root_freq = get_lcdif_root_clk(cscdr2);

	return root_freq / pred / podf;
}
#endif

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
#ifdef CONFIG_VIDEO_MXS
	case MXC_LCDIF_CLK:
		return get_lcdif_clk();
#endif
	default:
		printf("Unsupported MXC CLK: %d\n", clk);
	}

	return 0;
}

/* Config CPU clock */
static int set_arm_clk(u32 ref, u32 freq_khz)
{
	int ret;
	int d;
	int div = 0;
	int mul = 0;
	u32 min_err = ~0;
	u32 reg;
	const int min_div = 54;
	const int max_div = 108;
	const int div_mask = 0x7f;
	const u32 max_freq = ref * max_div / 2;
	const u32 min_freq = ref * min_div / 8 / 2;

	if (freq_khz > max_freq / 1000 || freq_khz < min_freq / 1000) {
		printf("Frequency %u.%03uMHz is out of range: %u.%03u..%u.%03u\n",
			freq_khz / 1000, freq_khz % 1000,
			min_freq / 1000000, min_freq / 1000 % 1000,
			max_freq / 1000000, max_freq / 1000 % 1000);
		return -EINVAL;
	}

	for (d = DIV_ROUND_UP(648000, freq_khz); d <= 8; d++) {
		int m = freq_khz * 2 * d / (ref / 1000);
		u32 f;
		u32 err;

		if (m > max_div) {
			debug("%s@%d: d=%d m=%d\n", __func__, __LINE__,
				d, m);
			break;
		}

		f = ref * m / d / 2;
		if (f > freq_khz * 1000) {
			debug("%s@%d: d=%d m=%d f=%u freq=%u\n", __func__, __LINE__,
				d, m, f, freq_khz);
			if (--m < min_div)
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
	setbits_le32(&anatop->pll_video, BM_ANADIG_PLL_ARM_BYPASS);

	reg = (reg & ~div_mask) | mul;
	writel(reg, &anatop->pll_arm);

	writel(div - 1, &imx_ccm->cacrr);

	ret = wait_pll_lock(&anatop->pll_video);
	if (ret) {
		printf("ARM PLL failed to lock\n");
		return ret;
	}

	clrbits_le32(&anatop->pll_video, BM_ANADIG_PLL_ARM_BYPASS);

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
	print_pll(PLL_528);
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
#ifdef CONFIG_VIDEO_MXS
	print_clk(LCDIF);
#endif
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
				ref = MXC_HCLK;
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

#ifndef CONFIG_SOC_MX6SX
void enable_ipu_clock(void)
{
	int reg = readl(&imx_ccm->CCGR3);
	reg |= MXC_CCM_CCGR3_IPU1_IPU_MASK;
	writel(reg, &imx_ccm->CCGR3);

	if (is_mx6dqp()) {
		setbits_le32(&imx_ccm->CCGR6, MXC_CCM_CCGR6_PRG_CLK0_MASK);
		setbits_le32(&imx_ccm->CCGR3, MXC_CCM_CCGR3_IPU2_IPU_MASK);
	}
}
#endif
/***************************************************/

U_BOOT_CMD(
	clocks,	4, 0, do_clocks,
	"display/set clocks",
	"                    - display clock settings\n"
	"clocks <clkname> <freq>    - set clock <clkname> to <freq> MHz"
);
