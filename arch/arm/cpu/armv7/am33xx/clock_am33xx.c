/*
 * clock_am33xx.c
 *
 * clocks for AM33XX based boards
 *
 * Copyright (C) 2013, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <asm/arch/hardware.h>
#include <asm/io.h>

#define PRCM_MOD_EN		0x2
#define PRCM_FORCE_WAKEUP	0x2
#define PRCM_FUNCTL		0x0

#define PRCM_EMIF_CLK_ACTIVITY	BIT(2)
#define PRCM_L3_GCLK_ACTIVITY	BIT(4)

#define PLL_BYPASS_MODE		0x4
#define ST_MN_BYPASS		0x00000100
#define ST_DPLL_CLK		0x00000001
#define CLK_SEL_MASK		0x7ffff
#define CLK_DIV_MASK		0x1f
#define CLK_DIV2_MASK		0x7f
#define CLK_SEL_SHIFT		0x8
#define CLK_MODE_MASK		0x7
#define CLK_MODE_SEL		0x7
#define DPLL_CLKDCOLDO_GATE_CTRL 0x300

#define OSC	(V_OSCK/1000000)

#define MPUPLL_M	CONFIG_SYS_MPUCLK
#define MPUPLL_N	(OSC - 1)
#define MPUPLL_M2	1

/* Core PLL Fdll = 1 GHZ, */
#define COREPLL_M	1000
#define COREPLL_N	(OSC - 1)

#define COREPLL_M4	10	/* CORE_CLKOUTM4 = 200 MHZ */
#define COREPLL_M5	8	/* CORE_CLKOUTM5 = 250 MHZ */
#define COREPLL_M6	4	/* CORE_CLKOUTM6 = 500 MHZ */

/*
 * USB PHY clock is 960 MHZ. Since, this comes directly from Fdll, Fdll
 * frequency needs to be set to 960 MHZ. Hence,
 * For clkout = 192 MHZ, Fdll = 960 MHZ, divider values are given below
 */
#define PERPLL_M	960
#define PERPLL_N	(OSC - 1)
#define PERPLL_M2	5

/* DDR Freq is 266 MHZ for now */
/* Set Fdll = 400 MHZ , Fdll = M * 2 * CLKINP/ N + 1; clkout = Fdll /(2 * M2) */
#define DDRPLL_M	266
#define DDRPLL_N	(OSC - 1)
#define DDRPLL_M2	1

struct cm_perpll *const cmper = (struct cm_perpll *)CM_PER;
struct cm_wkuppll *const cmwkup = (struct cm_wkuppll *)CM_WKUP;
struct cm_dpll *const cmdpll = (struct cm_dpll *)CM_DPLL;
struct cm_rtc *const cmrtc = (struct cm_rtc *)CM_RTC;

const struct dpll_regs dpll_mpu_regs = {
	.cm_clkmode_dpll	= CM_WKUP + 0x88,
	.cm_idlest_dpll		= CM_WKUP + 0x20,
	.cm_clksel_dpll		= CM_WKUP + 0x2C,
	.cm_div_m2_dpll		= CM_WKUP + 0xA8,
};

const struct dpll_regs dpll_core_regs = {
	.cm_clkmode_dpll	= CM_WKUP + 0x90,
	.cm_idlest_dpll		= CM_WKUP + 0x5C,
	.cm_clksel_dpll		= CM_WKUP + 0x68,
	.cm_div_m4_dpll		= CM_WKUP + 0x80,
	.cm_div_m5_dpll		= CM_WKUP + 0x84,
	.cm_div_m6_dpll		= CM_WKUP + 0xD8,
};

const struct dpll_regs dpll_per_regs = {
	.cm_clkmode_dpll	= CM_WKUP + 0x8C,
	.cm_idlest_dpll		= CM_WKUP + 0x70,
	.cm_clksel_dpll		= CM_WKUP + 0x9C,
	.cm_div_m2_dpll		= CM_WKUP + 0xAC,
};

const struct dpll_regs dpll_ddr_regs = {
	.cm_clkmode_dpll	= CM_WKUP + 0x94,
	.cm_idlest_dpll		= CM_WKUP + 0x34,
	.cm_clksel_dpll		= CM_WKUP + 0x40,
	.cm_div_m2_dpll		= CM_WKUP + 0xA0,
};

struct dpll_params dpll_mpu_opp100 = {
		CONFIG_SYS_MPUCLK, OSC-1, 1, -1, -1, -1, -1};
const struct dpll_params dpll_core_opp100 = {
		1000, OSC-1, -1, -1, 10, 8, 4};
const struct dpll_params dpll_mpu = {
		MPUPLL_M_300, OSC-1, 1, -1, -1, -1, -1};
const struct dpll_params dpll_core = {
		50, OSC-1, -1, -1, 1, 1, 1};
const struct dpll_params dpll_per = {
		960, OSC-1, 5, -1, -1, -1, -1};

const struct dpll_params *get_dpll_mpu_params(void)
{
	return &dpll_mpu;
}

const struct dpll_params *get_dpll_core_params(void)
{
	return &dpll_core;
}

const struct dpll_params *get_dpll_per_params(void)
{
	return &dpll_per;
}

void setup_clocks_for_console(void)
{
	clrsetbits_le32(&cmwkup->wkclkstctrl, CD_CLKCTRL_CLKTRCTRL_MASK,
			CD_CLKCTRL_CLKTRCTRL_SW_WKUP <<
			CD_CLKCTRL_CLKTRCTRL_SHIFT);

	clrsetbits_le32(&cmper->l4hsclkstctrl, CD_CLKCTRL_CLKTRCTRL_MASK,
			CD_CLKCTRL_CLKTRCTRL_SW_WKUP <<
			CD_CLKCTRL_CLKTRCTRL_SHIFT);

	clrsetbits_le32(&cmwkup->wkup_uart0ctrl,
			MODULE_CLKCTRL_MODULEMODE_MASK,
			MODULE_CLKCTRL_MODULEMODE_SW_EXPLICIT_EN <<
			MODULE_CLKCTRL_MODULEMODE_SHIFT);
	clrsetbits_le32(&cmper->uart1clkctrl,
			MODULE_CLKCTRL_MODULEMODE_MASK,
			MODULE_CLKCTRL_MODULEMODE_SW_EXPLICIT_EN <<
			MODULE_CLKCTRL_MODULEMODE_SHIFT);
	clrsetbits_le32(&cmper->uart2clkctrl,
			MODULE_CLKCTRL_MODULEMODE_MASK,
			MODULE_CLKCTRL_MODULEMODE_SW_EXPLICIT_EN <<
			MODULE_CLKCTRL_MODULEMODE_SHIFT);
	clrsetbits_le32(&cmper->uart3clkctrl,
			MODULE_CLKCTRL_MODULEMODE_MASK,
			MODULE_CLKCTRL_MODULEMODE_SW_EXPLICIT_EN <<
			MODULE_CLKCTRL_MODULEMODE_SHIFT);
	clrsetbits_le32(&cmper->uart4clkctrl,
			MODULE_CLKCTRL_MODULEMODE_MASK,
			MODULE_CLKCTRL_MODULEMODE_SW_EXPLICIT_EN <<
			MODULE_CLKCTRL_MODULEMODE_SHIFT);
	clrsetbits_le32(&cmper->uart5clkctrl,
			MODULE_CLKCTRL_MODULEMODE_MASK,
			MODULE_CLKCTRL_MODULEMODE_SW_EXPLICIT_EN <<
			MODULE_CLKCTRL_MODULEMODE_SHIFT);
}

void enable_basic_clocks(void)
{
	u32 *const clk_domains[] = {
		&cmper->l3clkstctrl,
		&cmper->l4fwclkstctrl,
		&cmper->l3sclkstctrl,
		&cmper->l4lsclkstctrl,
		&cmwkup->wkclkstctrl,
		&cmper->emiffwclkctrl,
		&cmrtc->clkstctrl,
		0
	};

	u32 *const clk_modules_explicit_en[] = {
		&cmper->l3clkctrl,
		&cmper->l4lsclkctrl,
		&cmper->l4fwclkctrl,
		&cmwkup->wkl4wkclkctrl,
		&cmper->l3instrclkctrl,
		&cmper->l4hsclkctrl,
		&cmwkup->wkgpio0clkctrl,
		&cmwkup->wkctrlclkctrl,
		&cmper->timer2clkctrl,
		&cmper->gpmcclkctrl,
		&cmper->elmclkctrl,
		&cmper->mmc0clkctrl,
		&cmper->mmc1clkctrl,
		&cmwkup->wkup_i2c0ctrl,
		&cmper->gpio1clkctrl,
		&cmper->gpio2clkctrl,
		&cmper->gpio3clkctrl,
		&cmper->i2c1clkctrl,
		&cmper->cpgmac0clkctrl,
		&cmper->spi0clkctrl,
		&cmrtc->rtcclkctrl,
		&cmper->usb0clkctrl,
		&cmper->emiffwclkctrl,
		&cmper->emifclkctrl,
		0
	};

	do_enable_clocks(clk_domains, clk_modules_explicit_en, 1);

	/* Select the Master osc 24 MHZ as Timer2 clock source */
	writel(0x1, &cmdpll->clktimer2clk);
}
void mpu_pll_config_val(int mpull_m)
{
	u32 clkmode, clksel, div_m2;

	clkmode = readl(&cmwkup->clkmoddpllmpu);
	clksel = readl(&cmwkup->clkseldpllmpu);
	div_m2 = readl(&cmwkup->divm2dpllmpu);

	/* Set the PLL to bypass Mode */
	writel(PLL_BYPASS_MODE, &cmwkup->clkmoddpllmpu);
	while (readl(&cmwkup->idlestdpllmpu) != ST_MN_BYPASS)
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= (mpull_m << CLK_SEL_SHIFT) | MPUPLL_N;
	writel(clksel, &cmwkup->clkseldpllmpu);

	div_m2 &= ~CLK_DIV_MASK;
	div_m2 |= MPUPLL_M2;
	writel(div_m2, &cmwkup->divm2dpllmpu);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddpllmpu);

	while (readl(&cmwkup->idlestdpllmpu) != ST_DPLL_CLK)
		;
}

void mpu_pll_config(void)
{
	mpu_pll_config_val(CONFIG_SYS_MPUCLK);
}

static void core_pll_config_val(int m)
{
	u32 clkmode, clksel, div_m4, div_m5, div_m6;

	clkmode = readl(&cmwkup->clkmoddpllcore);
	clksel = readl(&cmwkup->clkseldpllcore);
	div_m4 = readl(&cmwkup->divm4dpllcore);
	div_m5 = readl(&cmwkup->divm5dpllcore);
	div_m6 = readl(&cmwkup->divm6dpllcore);

	/* Set the PLL to bypass Mode */
	writel(PLL_BYPASS_MODE, &cmwkup->clkmoddpllcore);

	while (readl(&cmwkup->idlestdpllcore) != ST_MN_BYPASS)
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= ((m << CLK_SEL_SHIFT) | COREPLL_N);
	writel(clksel, &cmwkup->clkseldpllcore);

	div_m4 &= ~CLK_DIV_MASK;
	div_m4 |= COREPLL_M4;
	writel(div_m4, &cmwkup->divm4dpllcore);

	div_m5 &= ~CLK_DIV_MASK;
	div_m5 |= COREPLL_M5;
	writel(div_m5, &cmwkup->divm5dpllcore);

	div_m6 &= ~CLK_DIV_MASK;
	div_m6 |= COREPLL_M6;
	writel(div_m6, &cmwkup->divm6dpllcore);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddpllcore);

	while (readl(&cmwkup->idlestdpllcore) != ST_DPLL_CLK)
		;
}

static inline void core_pll_config(void)
{
	core_pll_config_val(COREPLL_M);
}

static void per_pll_config_val(int m)
{
	u32 clkmode, clksel, div_m2;

	clkmode = readl(&cmwkup->clkmoddpllper);
	clksel = readl(&cmwkup->clkseldpllper);
	div_m2 = readl(&cmwkup->divm2dpllper);

	/* Set the PLL to bypass Mode */
	writel(PLL_BYPASS_MODE, &cmwkup->clkmoddpllper);

	while (readl(&cmwkup->idlestdpllper) != ST_MN_BYPASS)
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= (m << CLK_SEL_SHIFT) | PERPLL_N;
	writel(clksel, &cmwkup->clkseldpllper);

	div_m2 &= ~CLK_DIV2_MASK;
	div_m2 |= PERPLL_M2;
	writel(div_m2, &cmwkup->divm2dpllper);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddpllper);

	while (readl(&cmwkup->idlestdpllper) != ST_DPLL_CLK)
		;

	writel(DPLL_CLKDCOLDO_GATE_CTRL, &cmwkup->clkdcoldodpllper);
}

static inline void per_pll_config(void)
{
	per_pll_config_val(PERPLL_M);
}

static void disp_pll_config_val(int m)
{
	u32 clkmode, clksel, div_m2;

	clkmode = readl(&cmwkup->clkmoddplldisp);
	clksel = readl(&cmwkup->clkseldplldisp);
	div_m2 = readl(&cmwkup->divm2dplldisp);

	/* Set the PLL to bypass Mode */
	writel(PLL_BYPASS_MODE, &cmwkup->clkmoddplldisp);

	while (!(readl(&cmwkup->idlestdplldisp) & ST_MN_BYPASS))
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= (m << CLK_SEL_SHIFT) | DISPPLL_N;
	writel(clksel, &cmwkup->clkseldplldisp);

	div_m2 &= ~CLK_DIV2_MASK;
	div_m2 |= DISPPLL_M2;
	writel(div_m2, &cmwkup->divm2dplldisp);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddplldisp);

	while (!(readl(&cmwkup->idlestdplldisp) & ST_DPLL_CLK))
		;
}

static inline void disp_pll_config(void)
{
	disp_pll_config_val(DISPPLL_M);
}

void ddr_pll_config(unsigned int ddrpll_m)
{
	u32 clkmode, clksel, div_m2;

	clkmode = readl(&cmwkup->clkmoddpllddr);
	clksel = readl(&cmwkup->clkseldpllddr);
	div_m2 = readl(&cmwkup->divm2dpllddr);

	/* Set the PLL to bypass Mode */
	clkmode &= ~CLK_MODE_MASK;
	clkmode |= PLL_BYPASS_MODE;
	writel(clkmode, &cmwkup->clkmoddpllddr);

	/* Wait till bypass mode is enabled */
	while (!(readl(&cmwkup->idlestdpllddr) & ST_MN_BYPASS))
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= (ddrpll_m << CLK_SEL_SHIFT) | DDRPLL_N;
	writel(clksel, &cmwkup->clkseldpllddr);

	div_m2 &= ~CLK_DIV_MASK;
	div_m2 |= DDRPLL_M2;
	writel(div_m2, &cmwkup->divm2dpllddr);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddpllddr);

	/* Wait till dpll is locked */
	while ((readl(&cmwkup->idlestdpllddr) & ST_DPLL_CLK) != ST_DPLL_CLK)
		;
}

#define M(mn) (((mn) & CLK_SEL_MASK) >> CLK_SEL_SHIFT)
#define N(mn) ((mn) & CLK_DIV2_MASK)

unsigned long __clk_get_rate(u32 m_n, u32 div_m2)
{
	unsigned long rate;

	div_m2 &= CLK_DIV_MASK;
	debug("M=%u N=%u M2=%u\n", M(m_n), N(m_n), div_m2);
	rate = V_OSCK / 1000 * M(m_n) / (N(m_n) + 1) / div_m2;
	debug("CLK = %lu.%03luMHz\n", rate / 1000, rate % 1000);
	return rate * 1000;
}

unsigned long lcdc_clk_rate(void)
{
	return clk_get_rate(cmwkup, disp);
}

unsigned long mpu_clk_rate(void)
{
	return clk_get_rate(cmwkup, mpu);
}

enum {
	CLK_MPU_PLL,
	CLK_CORE_PLL,
	CLK_PER_PLL,
	CLK_DISP_PLL,
	CLK_GPMC,
};

static struct clk_lookup {
	const char *name;
	unsigned int index;
} am33xx_clk_lookup[] = {
	{ "mpu", CLK_MPU_PLL, },
	{ "core", CLK_CORE_PLL, },
	{ "per", CLK_PER_PLL, },
	{ "lcdc", CLK_DISP_PLL, },
	{ "gpmc", CLK_GPMC, },
};

#define print_pll(dom, pll) {				\
	u32 __pll = clk_get_rate(dom, pll);		\
	printf("%-12s %4d.%03d MHz\n", #pll,		\
		__pll / 1000000, __pll / 1000 % 1000);	\
	}

#define print_pll2(dom, n, pll) {			\
	u32 __m_n = readl(&(dom)->clkseldpll##pll);	\
	u32 __div = readl(&(dom)->divm##n##dpll##pll);	\
	u32 __pll = __clk_get_rate(__m_n, __div);	\
	printf("%-12s %4d.%03d MHz\n", #pll "_m" #n,	\
		__pll / 1000000, __pll / 1000 % 1000);	\
	}

static void do_showclocks(void)
{
	print_pll(cmwkup, mpu);
	print_pll2(cmwkup, 4, core);
	print_pll2(cmwkup, 5, core);
	print_pll2(cmwkup, 6, core);
	print_pll(cmwkup, ddr);
	print_pll(cmwkup, per);
	print_pll(cmwkup, disp);
}

int do_clocks(cmd_tbl_t *cmdtp, int flag, int argc,
	char *const argv[])
{
	int i;
	unsigned long freq;
	unsigned long __attribute__((unused)) ref = ~0UL;

	if (argc < 2) {
		do_showclocks();
		return CMD_RET_SUCCESS;
	} else if (argc == 2 || argc > 4) {
		return CMD_RET_USAGE;
	}

	freq = simple_strtoul(argv[2], NULL, 0);
	if (freq < 1000) {
		printf("Invalid clock frequency %lu\n", freq);
		return CMD_RET_FAILURE;
	}
	if (argc > 3) {
		ref = simple_strtoul(argv[3], NULL, 0);
	}
	for (i = 0; i < ARRAY_SIZE(am33xx_clk_lookup); i++) {
		if (strcasecmp(argv[1], am33xx_clk_lookup[i].name) == 0) {
			switch (am33xx_clk_lookup[i].index) {
			case CLK_MPU_PLL:
				mpu_pll_config_val(freq / 1000000);
				break;
			case CLK_CORE_PLL:
				core_pll_config_val(freq / 1000000);
				break;
			case CLK_PER_PLL:
				per_pll_config_val(freq / 1000000);
				break;
			case CLK_DISP_PLL:
				disp_pll_config_val(freq / 1000000);
				break;
			default:
				printf("Cannot change %s clock\n",
					am33xx_clk_lookup[i].name);
				return CMD_RET_FAILURE;
			}

			printf("%s clock set to %lu.%03lu MHz\n",
				am33xx_clk_lookup[i].name,
				freq / 1000000, freq / 1000 % 1000);
			return CMD_RET_SUCCESS;
		}
	}
	if (i == ARRAY_SIZE(am33xx_clk_lookup)) {
		printf("clock %s not found; supported clocks are:\n", argv[1]);
		for (i = 0; i < ARRAY_SIZE(am33xx_clk_lookup); i++) {
			printf("\t%s\n", am33xx_clk_lookup[i].name);
		}
	} else {
		printf("Failed to set clock %s to %s MHz\n",
			argv[1], argv[2]);
	}
	return CMD_RET_FAILURE;
}

U_BOOT_CMD(
	clocks, 4, 0, do_clocks,
	"display/set clocks",
	"                    - display clock settings\n"
	"clocks <clkname> <freq>    - set clock <clkname> to <freq> Hz"
);
