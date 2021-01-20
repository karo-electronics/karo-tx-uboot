// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <errno.h>
#include <hang.h>
#include <i2c.h>
#include <spl.h>
#include <asm/io.h>
#include <dm/uclass.h>
#include <power/pmic.h>
#include <power/rn5t567_pmic.h>
#include "pmic.h"

#define PMIC_I2C_BUS			0
#define PMIC_I2C_ADDR			0x34
#define PMIC_NAME			"RN5T567"

#define RN5T567_REG(n, val, ...)	PMIC_REG_VAL(RN5T567, n, val)

#define NOETIMSET_DIS_OFF_NOE_TIM	BIT(3)

#define DCnCTL_EN			BIT(0)
#define DCnCTL_DIS			BIT(1)
#define DCnMODE(m)			(((m) & 0x3) << 4)
#define DCnMODE_SLP(m)			(((m) & 0x3) << 6)
#define MODE_AUTO			0
#define MODE_PWM			1
#define MODE_PSM			2

#define DCnCTL2_LIMSDEN			BIT(0)
#define DCnCTL2_LIM_NONE		(0 << 1)
#define DCnCTL2_LIM_LOW			BIT(1)
#define DCnCTL2_LIM_MED			(2 << 1)
#define DCnCTL2_LIM_HIGH		(3 << 1)
#define DCnCTL2_SR_LOW			(2 << 4)
#define DCnCTL2_SR_MED			BIT(4)
#define DCnCTL2_SR_HIGH			(0 << 4)
#define DCnCTL2_OSC_LOW			(0 << 6)
#define DCnCTL2_OSC_MED			BIT(6)
#define DCnCTL2_OSC_HIGH		(2 << 6)

#define LDOEN1_LDO1EN			BIT(0)
#define LDOEN1_LDO2EN			BIT(1)
#define LDOEN1_LDO3EN			BIT(2)
#define LDOEN1_LDO4EN			BIT(3)
#define LDOEN1_LDO5EN			BIT(4)
#define LDOEN1_VAL			(LDOEN1_LDO1EN | LDOEN1_LDO2EN | \
					 LDOEN1_LDO3EN | LDOEN1_LDO4EN | \
					 LDOEN1_LDO5EN)
#define LDOEN1_MASK			0x1f

#define LDOEN2_LDORTC1EN		BIT(4)
#define LDOEN2_LDORTC2EN		BIT(5)
#define LDOEN2_VAL			(LDOEN2_LDORTC1EN | LDOEN2_LDORTC2EN)
#define LDOEN2_MASK			0x30

/* calculate voltages in 10mV */
#define rn5t_v2r(v, n, m)		(((((v) * 10 < (n)) ? (n) :	\
					   (v) * 10) - (n)) / (m))
#define rn5t_r2v(r, n, m)		(((r) * (m) + (n)) / 10)

/* DCDC1-4 */
#define rn5t_mV_to_regval(mV)		rn5t_v2r(mV, 6000, 125)
#define rn5t_regval_to_mV(r)		rn5t_r2v(r, 6000, 125)

/* LDO1-2, 4-5 */
#define rn5t_mV_to_regval2(mV)		(rn5t_v2r(mV, 9000, 500) << 1)
#define rn5t_regval2_to_mV(r)		rn5t_r2v((r) >> 1, 9000, 500)

/* LDO3 */
#define rn5t_mV_to_regval3(mV)		(rn5t_v2r(mV, 6000, 500) << 1)
#define rn5t_regval3_to_mV(r)		rn5t_r2v((r) >> 1, 6000, 500)

/* LDORTC */
#define rn5t_mV_to_regval_rtc(mV)	(rn5t_v2r(mV, 12000, 500) << 1)
#define rn5t_regval_rtc_to_mV(r)	rn5t_r2v((r) >> 1, 12000, 500)

/* DCDC1 */
#define VDD_DRAM_PU_VAL			rn5t_mV_to_regval(900)
#define VDD_DRAM_PU_VAL_LP		rn5t_mV_to_regval(900)

/* DCDC2 */
#define VDD_ARM_VAL			rn5t_mV_to_regval(900)
#define VDD_ARM_VAL_LP			rn5t_mV_to_regval(900)

/* DCDC3 */
#define VDD_SOC_VAL			rn5t_mV_to_regval(900)
#define VDD_SOC_VAL_LP			rn5t_mV_to_regval(900)

/* DCDC4 */
#define NVCC_DRAM_VAL			rn5t_mV_to_regval(1350)
#define NVCC_DRAM_VAL_LP		rn5t_mV_to_regval(1350)

/* LDORTC1 */
#define NVCC_SNVS_1V8_VAL		rn5t_mV_to_regval_rtc(1800)

/* LDORTC2 */
#define VDD_SNVS_0V9_VAL		rn5t_mV_to_regval_rtc(900)

/* LDO1 */
#define VDD_3V3_VAL			rn5t_mV_to_regval2(3300)
#define VDD_3V3_VAL_LP			rn5t_mV_to_regval2(3300)
/* LDO2 */
#define VDDA_1V8_VAL			rn5t_mV_to_regval2(1800)
#define VDDA_1V8_VAL_LP			rn5t_mV_to_regval2(1800)
/* LDO3 */
#define VDD_1V8_VAL			rn5t_mV_to_regval3(1800)
#define VDD_1V8_VAL_LP			rn5t_mV_to_regval3(1800)
/* LDO4 */
#define VDD_PHY_1V2_VAL			rn5t_mV_to_regval2(1200)
#define VDD_PHY_1V2_VAL_LP		rn5t_mV_to_regval2(1200)
/* LDO5 */
#define VDD_PHY_0V9_VAL			rn5t_mV_to_regval2(900)
#define VDD_PHY_0V9_VAL_LP		rn5t_mV_to_regval2(900)

static const struct pmic_val pmic_vals[] = {
	RN5T567_REG(NOETIMSETCNT, NOETIMSET_DIS_OFF_NOE_TIM | 0x5),
	RN5T567_REG(SLPCNT, 0),
	RN5T567_REG(REPCNT, (3 << 4) | (0 << 1)),

	RN5T567_REG(LDORTC1DAC, NVCC_SNVS_1V8_VAL),
	RN5T567_REG(LDORTC2DAC, VDD_SNVS_0V9_VAL),

	RN5T567_REG(DC1DAC, VDD_ARM_VAL),
	RN5T567_REG(DC2DAC, VDD_DRAM_PU_VAL),
	RN5T567_REG(DC3DAC, VDD_SOC_VAL),
	RN5T567_REG(DC4DAC, NVCC_DRAM_VAL),

	RN5T567_REG(DC1DAC_SLP, VDD_ARM_VAL_LP),
	RN5T567_REG(DC2DAC_SLP, VDD_DRAM_PU_VAL_LP),
	RN5T567_REG(DC3DAC_SLP, VDD_SOC_VAL_LP),
	RN5T567_REG(DC4DAC_SLP, NVCC_DRAM_VAL_LP),

	RN5T567_REG(DC1CTL, DCnCTL_EN | DCnCTL_DIS),
	RN5T567_REG(DC2CTL, DCnCTL_EN | DCnCTL_DIS),
	RN5T567_REG(DC3CTL, DCnCTL_EN | DCnCTL_DIS),
	RN5T567_REG(DC4CTL, DCnCTL_EN | DCnCTL_DIS),
	RN5T567_REG(DC1CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_LOW |
		    DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW),
	RN5T567_REG(DC2CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_LOW |
		    DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW),
	RN5T567_REG(DC3CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_LOW |
		    DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW),
	RN5T567_REG(DC4CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_LOW |
		    DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW),

	RN5T567_REG(LDO1DAC, VDD_3V3_VAL),
	RN5T567_REG(LDO2DAC, VDDA_1V8_VAL),
	RN5T567_REG(LDO3DAC, VDD_1V8_VAL),
	RN5T567_REG(LDO4DAC, VDD_PHY_1V2_VAL),
	RN5T567_REG(LDO5DAC, VDD_PHY_0V9_VAL),

	RN5T567_REG(LDO1DAC_SLP, VDD_3V3_VAL_LP),
	RN5T567_REG(LDO2DAC_SLP, VDDA_1V8_VAL_LP),
	RN5T567_REG(LDO3DAC_SLP, VDD_1V8_VAL_LP),
	RN5T567_REG(LDO4DAC_SLP, VDD_PHY_1V2_VAL_LP),
	RN5T567_REG(LDO5DAC_SLP, VDD_PHY_0V9_VAL_LP),

	RN5T567_REG(LDORTC1_SLOT, 0x0f),
	RN5T567_REG(DC3_SLOT, 0x1f),
	RN5T567_REG(DC1_SLOT, 0x2f),
	RN5T567_REG(LDO5_SLOT, 0x3f),
	RN5T567_REG(DC2_SLOT, 0x4f),
	RN5T567_REG(LDO2_SLOT, 0x5f),
	RN5T567_REG(LDO3_SLOT, 0x6f),
	RN5T567_REG(DC4_SLOT, 0x7f),
	RN5T567_REG(LDO1_SLOT, 0x8f),
	RN5T567_REG(LDO4_SLOT, 0x9f),

	RN5T567_REG(LDOEN1, LDOEN1_VAL, ~LDOEN1_MASK),
	RN5T567_REG(LDOEN2, LDOEN2_VAL, ~LDOEN2_MASK),

	RN5T567_REG(LDODIS1, 0x1f, ~0x1f),
	RN5T567_REG(INTPOL, 0),
	RN5T567_REG(INTEN, 0x0),
	RN5T567_REG(DCIREN, 0x0),
	RN5T567_REG(EN_GPIR, 0),
};

#if IS_ENABLED(CONFIG_DM_I2C)
int power_init_board(void)
{
	int ret;
	size_t i;
	u8 ver[2];
	struct udevice *busdev;
	struct udevice *i2cdev;

	ret = uclass_get_device(UCLASS_I2C, PMIC_I2C_BUS, &busdev);
	if (ret) {
		printf("Failed to get I2C bus %d\n", PMIC_I2C_BUS);
		return ret;
	}

	ret = dm_i2c_probe(busdev, PMIC_I2C_ADDR, 0, &i2cdev);
	if (ret) {
		printf("Could not find PMIC @0x%02x: %d\n", PMIC_I2C_ADDR, ret);
		return ret;
	}

	ret = dm_i2c_read(i2cdev, RN5T567_LSIVER, ver, ARRAY_SIZE(ver));
	if (ret) {
		printf("Failed to read %s version registers: %d\n",
		       PMIC_NAME, ret);
		return ret;
	}

	for (i = 0; ret == 0 && i < ARRAY_SIZE(pmic_vals); i++) {
		const struct pmic_val *p = &pmic_vals[i];

		ret = pmic_update_reg(i2cdev, p->addr, p->val, p->mask, p->name);
	}

	if (CONFIG_IS_ENABLED(BANNER_PRINT))
		printf("PMIC:  %s LSI ver: 0x%02x OTP ver: 0x%02x %s\n",
		       PMIC_NAME, ver[0], ver[1], ret ? "FAILED" : "OK");
	else if (ret)
		printf("PMIC:  %s FAILED: %d\n", PMIC_NAME, ret);

	return ret;
}
#else
int power_init_board(void)
{
	int ret;
	size_t i;
	u8 ver[2];

	ret = i2c_probe(PMIC_I2C_ADDR);
	if (ret) {
		printf("Could not find PMIC @0x%02x: %d\n", PMIC_I2C_ADDR, ret);
		return ret;
	}

	ret = i2c_read(PMIC_I2C_ADDR, RN5T567_LSIVER, 1, ver, ARRAY_SIZE(ver));
	if (ret) {
		printf("Failed to read %s version registers: %d\n",
		       PMIC_NAME, ret);
		return ret;
	}

	for (i = 0; ret == 0 && i < ARRAY_SIZE(pmic_vals); i++) {
		const struct pmic_val *p = &pmic_vals[i];

		ret = pmic_update_reg(PMIC_I2C_ADDR,
				      p->addr, p->val, p->mask, p->name);
	}
	if (CONFIG_IS_ENABLED(BANNER_PRINT))
		printf("PMIC:  %s LSI ver: 0x%02x OTP ver: 0x%02x %s\n",
		       PMIC_NAME, ver[0], ver[1], ret ? "FAILED" : "OK");
	else if (ret)
		printf("PMIC:  %s FAILED: %d\n", PMIC_NAME, ret);

	return ret;
}
#endif /* CONFIG_DM_I2C */
