// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Ka-Ro electronics GmbH
 * RN5T567 PMIC setup for the Ka-Ro i.MX6 modules.
 *
 * Register tables from karo-tx-uboot board/karo/tx6/{tx6ul,tx6qdl}.c.
 * Chip loses register state on power loss; rewritten every boot.
 */
#include <dm.h>
#include <dm/uclass.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <power/pmic.h>
#include <power/rn5t567_pmic.h>
#include "karo_pmic.h"

#define NOETIMSET_DIS_OFF_NOE_TIM	BIT(3)

#define DCnCTL_EN		BIT(0)
#define DCnCTL_DIS		BIT(1)
#define DCnMODE_SLP(m)		(((m) & 0x3) << 6)
#define MODE_PSM		2

/*
 * DCnCTL2: only LIMSDEN and LIM_HIGH set bits; SR_HIGH and OSC_LOW
 * are the 0-encodings of their fields and are kept purely to document
 * that slew rate and oscillator frequency are deliberately left at
 * field value 0 (see full RN5T567 datasheet, DCnCTL2 description).
 */
#define DCnCTL2_LIMSDEN		BIT(0)
#define DCnCTL2_LIM_HIGH	(3 << 1)
#define DCnCTL2_SR_HIGH		(0 << 4)
#define DCnCTL2_OSC_LOW		(0 << 6)

/* voltage conversion (10 mV units), same formulas as master rn5t567.h */
#define rn5t_v2r(v, n, m)	DIV_ROUND_UP(((((v) * 10 < (n)) ? (n) : \
					(v) * 10) - (n)), m)
#define rn5t_r2v(r, n, m)	(((r) * (m) + (n)) / 10)
/* DCDC1-4 */
#define rn5t_mV_to_regval(mV)	rn5t_v2r(mV, 6000, 125)
#define rn5t_regval_to_mV(r)	rn5t_r2v(r, 6000, 125)
/* LDO1-2, 4-5 */
#define rn5t_mV_to_regval2(mV)	(rn5t_v2r(mV, 9000, 500) << 1)
/* LDO3 base is 0.6 V (master rn5t567.h regval3) */
#define rn5t_mV_to_regval3(mV)	(rn5t_v2r(mV, 6000, 500) << 1)
/* LDORTC */
#define rn5t_mV_to_regval_rtc(mV)	(rn5t_v2r(mV, 12000, 500) << 1)

#define VDD_RTC_VAL		rn5t_mV_to_regval_rtc(3000)

#if defined(CONFIG_MX6S) || defined(CONFIG_MX6DL) || defined(CONFIG_MX6Q)
/* TX6S/TX6U: DCDC2 supplies VDD_SOC, DCDC4 is VDD_HIGH at 3000 mV */
#define VDD_CORE_VAL		rn5t_mV_to_regval(1400)		/* DCDC1 */
#define VDD_CORE_VAL_LP		rn5t_mV_to_regval(900)
#define VDD_SOC_VAL		rn5t_mV_to_regval(1400)		/* DCDC2 */
#define VDD_SOC_VAL_LP		rn5t_mV_to_regval(1400)
#define VDD_DDR_VAL		rn5t_mV_to_regval(1350)		/* DCDC3 */
#define VDD_DDR_VAL_LP		rn5t_mV_to_regval(1350)
#define VDD_HIGH_VAL		rn5t_mV_to_regval(3000)		/* DCDC4 */
#define VDD_HIGH_VAL_LP		rn5t_mV_to_regval(3000)
#define VDD_IO_INT_VAL		rn5t_mV_to_regval2(3300)	/* LDO1 */
#define VDD_IO_EXT_VAL		rn5t_mV_to_regval2(3300)	/* LDO2 */
#else
#define LDOEN1_VAL		0x0f	/* UL/ULL: LDO1..4 on, LDO5 off */
#define LDOEN1_MASK		0x1f
#define LDOEN2_VAL		BIT(4)	/* LDORTC1 on */
#define LDOEN2_MASK		0x30
#if defined(CONFIG_MX6ULL)
/* IMX6ULLIEC Table 10: VDD_SOC_IN min 1.325 V, 792 MHz */
/* 1375 mV - 2 % stays above the minimum */
#define VDD_CORE_VAL		rn5t_mV_to_regval(1375)		/* DCDC1 */
#else
#define VDD_CORE_VAL		rn5t_mV_to_regval(1300)		/* DCDC1 */
#endif
#define VDD_CORE_VAL_LP		rn5t_mV_to_regval(900)
#define VDD_DDR_VAL		rn5t_mV_to_regval(1350)		/* DCDC3 */
#define VDD_DDR_VAL_LP		rn5t_mV_to_regval(1350)
#define VDD_IO_EXT_VAL		rn5t_mV_to_regval(3300)		/* DCDC4 */
#define VDD_IO_EXT_VAL_LP	rn5t_mV_to_regval(3300)
#define VDD_IO_INT_VAL		rn5t_mV_to_regval2(3300)	/* LDO1 */
#define VDD_IO_INT_VAL_LP	rn5t_mV_to_regval2(3300)
#define VDD_ADC_VAL		rn5t_mV_to_regval2(3300)	/* LDO2 */
#define VDD_ADC_VAL_LP		rn5t_mV_to_regval2(3300)
#define VDD_PMIC_VAL		rn5t_mV_to_regval3(2500)	/* LDO3 */
#define VDD_PMIC_VAL_LP		rn5t_mV_to_regval3(2500)
#define VDD_CSI_VAL		rn5t_mV_to_regval2(3300)	/* LDO4 */
#define VDD_CSI_VAL_LP		rn5t_mV_to_regval2(3300)
#define VDD_LDO5_VAL		rn5t_mV_to_regval2(1200)	/* LDO5 */
#endif

struct karo_pmic_reg {
	u8 addr;
	u8 val;
	u8 preserve_mask;	/* bits to keep from current value */
};

/* register set of master b634fa6430e ("all relevant PMIC registers") */
static const struct karo_pmic_reg rn5t567_regs[] = {
#if defined(CONFIG_MX6S) || defined(CONFIG_MX6DL) || defined(CONFIG_MX6Q)
	/*
	 * Same order and values as master tx6qdl.c rn5t567_regs[].
	 * Note: DCnCTL2 (current limit / slew rate) is intentionally not
	 * written here; the QDL modules rely on the OTP defaults, matching
	 * the legacy master code.
	 */
	{ RN5T567_NOETIMSETCNT, 0x5, },
	{ RN5T567_DC1DAC, VDD_CORE_VAL, },
	{ RN5T567_DC2DAC, VDD_SOC_VAL, },
	{ RN5T567_DC3DAC, VDD_DDR_VAL, },
	{ RN5T567_DC4DAC, VDD_HIGH_VAL, },
	{ RN5T567_DC1DAC_SLP, VDD_CORE_VAL_LP, },
	{ RN5T567_DC2DAC_SLP, VDD_SOC_VAL_LP, },
	{ RN5T567_DC3DAC_SLP, VDD_DDR_VAL_LP, },
	{ RN5T567_DC4DAC_SLP, VDD_HIGH_VAL_LP, },
	{ RN5T567_DC1CTL, DCnCTL_EN | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC2CTL, DCnCTL_EN | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC3CTL, DCnCTL_EN | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC4CTL, DCnCTL_EN | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_LDORTC1DAC, VDD_RTC_VAL, },
	{ RN5T567_LDORTC1_SLOT, 0x0f, (u8)~0x3f, },
	{ RN5T567_LDO1DAC, VDD_IO_INT_VAL, },
	{ RN5T567_LDO2DAC, VDD_IO_EXT_VAL, },
	{ RN5T567_LDOEN1, 0x03, (u8)~0x1f, },
	{ RN5T567_LDOEN2, 0x10, (u8)~0x30, },
	{ RN5T567_LDODIS1, 0x1c, (u8)~0x1f, },
	{ RN5T567_INTPOL, 0, },
	{ RN5T567_INTEN, 0x3, },
	{ RN5T567_DCIREN, 0xf, },
	{ RN5T567_EN_GPIR, 0, },
#else
	/* master tx6ul.c table minus duplicate _SLP rows */
	{ RN5T567_NOETIMSETCNT, NOETIMSET_DIS_OFF_NOE_TIM | 0x5, },
	{ RN5T567_SLPCNT, 0, },
	{ RN5T567_REPCNT, (3 << 4) | (0 << 1), },
	{ RN5T567_DC1DAC, VDD_CORE_VAL, },
	{ RN5T567_DC3DAC, VDD_DDR_VAL, },
	{ RN5T567_DC4DAC, VDD_IO_EXT_VAL, },
	{ RN5T567_DC1DAC_SLP, VDD_CORE_VAL_LP, },
	{ RN5T567_DC3DAC_SLP, VDD_DDR_VAL_LP, },
	{ RN5T567_DC4DAC_SLP, VDD_IO_EXT_VAL_LP, },
	{ RN5T567_DC1CTL, DCnCTL_EN | DCnCTL_DIS | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC2CTL, DCnCTL_DIS, },
	{ RN5T567_DC3CTL, DCnCTL_EN | DCnCTL_DIS | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC4CTL, DCnCTL_EN | DCnCTL_DIS | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC1CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_HIGH |
			   DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW, },
	/*
	 * DCDC2 is disabled above; its CTL2 is still written to match the
	 * master table so limits are sane if the rail is ever enabled.
	 */
	{ RN5T567_DC2CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_HIGH |
			   DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW, },
	{ RN5T567_DC3CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_HIGH |
			   DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW, },
	{ RN5T567_DC4CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_HIGH |
			   DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW, },
	{ RN5T567_LDORTC1DAC, VDD_RTC_VAL, },
	{ RN5T567_LDORTC1_SLOT, 0x0f, (u8)~0x3f, },
	{ RN5T567_LDO1DAC, VDD_IO_INT_VAL, },
	{ RN5T567_LDO2DAC, VDD_ADC_VAL, },
	{ RN5T567_LDO3DAC, VDD_PMIC_VAL, },
	{ RN5T567_LDO4DAC, VDD_CSI_VAL, },
	{ RN5T567_LDO5DAC, VDD_LDO5_VAL, },
	{ RN5T567_LDO1DAC_SLP, VDD_IO_INT_VAL_LP, },
	{ RN5T567_LDO2DAC_SLP, VDD_ADC_VAL_LP, },
	{ RN5T567_LDO3DAC_SLP, VDD_PMIC_VAL_LP, },
	{ RN5T567_LDO4DAC_SLP, VDD_CSI_VAL_LP, },
	/* no LDO5DAC_SLP: LDO5 is disabled (LDOEN1_VAL) */
	{ RN5T567_LDOEN1, LDOEN1_VAL, (u8)~LDOEN1_MASK, },
	{ RN5T567_LDOEN2, LDOEN2_VAL, (u8)~LDOEN2_MASK, },
	{ RN5T567_LDODIS1, 0x1f, (u8)~0x1f, },
	{ RN5T567_INTPOL, 0, },
	{ RN5T567_INTEN, 0x3, },
	{ RN5T567_DCIREN, 0xf, },
	{ RN5T567_EN_GPIR, 0, },
#endif
};

int karo_pmic_setup(void)
{
	struct udevice *dev;
	int ret;
	int i;

	ret = uclass_first_device_err(UCLASS_PMIC, &dev);
	if (ret) {
		printf("PMIC: no device found: %d\n", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(rn5t567_regs); i++) {
		const struct karo_pmic_reg *r = &rn5t567_regs[i];
		uint val = r->val;

		if (r->preserve_mask) {
			ret = pmic_reg_read(dev, r->addr);
			if (ret < 0) {
				printf("PMIC: read reg 0x%02x failed: %d\n",
				       r->addr, ret);
				return ret;
			}
			val |= ret & r->preserve_mask;
		}
		ret = pmic_reg_write(dev, r->addr, val);
		if (ret) {
			printf("PMIC: write reg 0x%02x failed: %d\n",
			       r->addr, ret);
			return ret;
		}
	}

	ret = pmic_reg_read(dev, RN5T567_DC1DAC);
	if (ret >= 0)
		debug("VDDCORE set to %umV\n", rn5t_regval_to_mV(ret));
	if (ret < 0)
		printf("PMIC:  RN5T567 FAILED: %d\n", ret);
	else
		printf("PMIC:  RN5T567 OK\n");

	return ret < 0 ? ret : 0;
}
