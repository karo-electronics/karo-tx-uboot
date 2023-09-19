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
#include <power/pmic.h>
#include <power/pca9450.h>
#include "pmic.h"

#ifdef CONFIG_KARO_QSXM_MM60
#define PMIC_NAME	"PCA9450A"
#else
#define PMIC_NAME	"PCA9450B"
#endif
#define PMIC_NODE	"pmic@25"

#define PCA9450_REG(n, val) PMIC_REG_VAL(PCA9450, n, val)

#define pca9450_mV_to_reg(mV)	(((mV) - 600) * 10 / 125)
#define pca9450_reg_to_mV(val)	(((val) * 125 / 10) + 600)

#define VDD_SOC_VAL			pca9450_mV_to_reg(950)
#define VDD_SOC_SLP_VAL			pca9450_mV_to_reg(850)
#define VDD_ARM_VAL			pca9450_mV_to_reg(950)
#define VDD_DRAM_VAL			pca9450_mV_to_reg(950)

static const struct pmic_val pmic_vals[] = {
	PCA9450_REG(BUCK123_DVS, 0x29),
	PCA9450_REG(BUCK1OUT_DVS0, VDD_SOC_VAL),
	PCA9450_REG(BUCK1OUT_DVS1, VDD_SOC_SLP_VAL),
	PCA9450_REG(BUCK2OUT_DVS0, VDD_ARM_VAL),
	PCA9450_REG(BUCK3OUT_DVS0, VDD_DRAM_VAL),
	PCA9450_REG(BUCK1CTRL, 0x59),
	PCA9450_REG(RESET_CTRL, 0xA1),
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
