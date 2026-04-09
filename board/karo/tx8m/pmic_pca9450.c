// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <errno.h>
#include <hang.h>
#include <i2c.h>
#include <spl.h>
#include <asm/io.h>
#include <power/pmic.h>
#include <power/pca9450.h>
#include "pmic.h"

#ifdef CONFIG_KARO_QSXM_MM60
#define PMIC_NAME	"PCA9450A"
#else
#define PMIC_NAME	"PCA9450B"
#endif
#define PMIC_NODE	"pmic@25"

#define PCA9450_REG(n, val, ...)	PMIC_REG_VAL(PCA9450, n, val, __VA_ARGS__)

#define pca9450_mV_to_reg(mV)		(((mV) - 600) * 10 / 125)
#define pca9450_reg_to_mV(val)		(((val) * 125 / 10) + 600)

#define VDD_SOC_VAL			pca9450_mV_to_reg(950)
#define VDD_SOC_SLP_VAL			pca9450_mV_to_reg(850)
#define VDD_ARM_VAL			pca9450_mV_to_reg(950)
#define VDD_DRAM_VAL			pca9450_mV_to_reg(950)

static const struct pmic_val pmic_vals[] = {
	/* reset all register to default values */
	PCA9450_REG(SW_RST, 0x05),
	PCA9450_REG(PWR_CTRL, 0x4c),
	/* 0.8125v for Overdrive mode */
	PCA9450_REG(BUCK1OUT_DVS0, VDD_SOC_VAL),
	PCA9450_REG(BUCK1OUT_DVS1, VDD_SOC_SLP_VAL),
	PCA9450_REG(BUCK2OUT_DVS0, VDD_ARM_VAL),
	/* disable buck2 in standby mode */
	PCA9450_REG(BUCK2OUT_DVS1, 0x4a),
#if IS_ENABLED(CONFIG_KARO_QSXM_MM60)
	PCA9450_REG(BUCK3OUT_DVS0, VDD_DRAM_VAL),
#endif
	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	PCA9450_REG(BUCK123_DVS, 0x00, 0x7f),

	/* enable DVS control through PMIC_STBY_REQ */
	PCA9450_REG(BUCK1CTRL, 0x59),
#if IS_ENABLED(CONFIG_KARO_QSXM_MM60)
	PCA9450_REG(BUCK3CTRL, 0x4a),
#endif
	PCA9450_REG(LDO2CTRL, 0xc0),
	/* switch off unused LDOs */
#if !IS_ENABLED(CONFIG_KARO_QSXM_MM60)
	PCA9450_REG(LDO4CTRL, 0x00),
#else
	PCA9450_REG(LDO4CTRL, 0x81),
	PCA9450_REG(LDO5CTRL_L, 0x00),
#endif
	/* set WDOG_B_CFG to cold reset */
	PCA9450_REG(RESET_CTRL, 0xa1),
};

int power_init_board(void)
{
	int ret;
	struct udevice *dev;
	size_t i;

	ret = pmic_get(PMIC_NODE, &dev);
	if (ret) {
		printf("Could not get '%s' PMIC: %d\n", PMIC_NODE, ret);
		return -ENODEV;
	}

	for (i = 0; ret == 0 && i < ARRAY_SIZE(pmic_vals); i++) {
		const struct pmic_val *r = &pmic_vals[i];

		ret = pmic_update_reg(dev, r->addr, r->val, r->mask, r->name);
	}

	if (ret)
		printf("PMIC:  %s FAILED: %d\n", PMIC_NAME, ret);
	else if (CONFIG_IS_ENABLED(BANNER_PRINT))
		printf("PMIC:  %s OK\n", PMIC_NAME);

	return ret;
}
