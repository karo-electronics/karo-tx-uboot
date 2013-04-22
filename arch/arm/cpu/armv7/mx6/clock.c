/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
//#define DEBUG

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

	reg = __raw_readl(&imx_ccm->cscdr1);
	reg &= MXC_CCM_CSCDR1_UART_CLK_PODF_MASK;
	uart_podf = reg >> MXC_CCM_CSCDR1_UART_CLK_PODF_OFFSET;

	return PLL3_80M / (uart_podf + 1);
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

static u32 get_mmdc_ch0_clk(void)
{
	u32 cbcdr = __raw_readl(&imx_ccm->cbcdr);
	u32 mmdc_ch0_podf = (cbcdr & MXC_CCM_CBCDR_MMDC_CH0_PODF_MASK) >>
				MXC_CCM_CBCDR_MMDC_CH0_PODF_OFFSET;

	return get_periph_clk() / (mmdc_ch0_podf + 1);
}

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
	reg |= MXC_CCM_CCGR3_CG0_MASK;
	writel(reg, &imx_ccm->CCGR3);
}

void ipu_clk_disable(void)
{
	u32 reg = readl(&imx_ccm->CCGR3);
	reg &= ~MXC_CCM_CCGR3_CG0_MASK;
	writel(reg, &imx_ccm->CCGR3);
}

void ocotp_clk_enable(void)
{
	u32 reg = readl(&imx_ccm->CCGR2);
	reg |= MXC_CCM_CCGR2_CG6_MASK;
	writel(reg, &imx_ccm->CCGR2);
}

void ocotp_clk_disable(void)
{
	u32 reg = readl(&imx_ccm->CCGR2);
	reg &= ~MXC_CCM_CCGR2_CG6_MASK;
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
static int config_core_clk(u32 ref, u32 freq)
{
	int d;
	int div = 0;
	int mul = 0;
	int min_err = ~0 >> 1;
	u32 reg;

	if (freq / ref > 108 || freq / ref * 8 < 54) {
		return -EINVAL;
	}

	for (d = 1; d < 8; d++) {
		int m = (freq + (ref - 1)) / ref;
		unsigned long f;
		int err;

		if (m > 108 || m < 54)
			return -EINVAL;

		f = ref * m / d;
		while (f > freq) {
			if (--m < 54)
				return -EINVAL;
			f = ref * m / d;
		}
		err = freq - f;
		if (err == 0)
			break;
		if (err < 0)
			return -EINVAL;
		if (err < min_err) {
			mul = m;
			div = d;
		}
	}
	printf("Setting M=%3u D=%2u for %u.%03uMHz (actual: %u.%03uMHz)\n",
		mul, div, freq / 1000000, freq / 1000 % 1000,
		ref * mul / div / 1000000, ref * mul / div / 1000 % 1000);

	reg = readl(&anatop->pll_arm);
	printf("anadig_pll_arm=%08x -> %08x\n",
		reg, (reg & ~0x7f) | mul);
#if 0
	writel(div - 1, &imx_ccm->caccr);
	reg &= 0x7f;
	writel(reg | mul, &anatop->pll_arm);
#endif
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
	freq *= 1000000;

	switch (clk) {
	case MXC_ARM_CLK:
		if (config_core_clk(ref, freq))
			return -EINVAL;
		break;
#if 0
	case MXC_PER_CLK:
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
#endif
	default:
		printf("Warning: Unsupported or invalid clock type: %d\n",
			clk);
		return -EINVAL;
	}

	return 0;
}

/*
 * Dump some core clocks.
 */
#define print_pll(pll)	printf("%-12s %4d.%03d MHz\n", #pll,		\
				decode_pll(pll, MXC_HCLK) / 1000000,	\
				decode_pll(pll, MXC_HCLK) / 1000 % 1000)

#define MXC_IPG_PER_CLK	MXC_IPG_PERCLK
#define print_clk(clk)	printf("%-12s %4d.%03d MHz\n", #clk,		\
				mxc_get_clock(MXC_##clk##_CLK) / 1000000, \
				mxc_get_clock(MXC_##clk##_CLK) / 1000 % 1000)

int do_mx6_showclocks(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	print_pll(PLL_ARM);
	print_pll(PLL_BUS);
	print_pll(PLL_USBOTG);
	print_pll(PLL_AUDIO);
	print_pll(PLL_VIDEO);
	print_pll(PLL_ENET);
	print_pll(PLL_USB2);

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

	return 0;
}

/***************************************************/

U_BOOT_CMD(
	clocks,	CONFIG_SYS_MAXARGS, 1, do_mx6_showclocks,
	"display clocks",
	""
);
