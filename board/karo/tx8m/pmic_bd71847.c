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
#include <power/bd71837.h>
#include "pmic.h"

#define BD71847_REG(n, val, ...)	PMIC_REG_VAL(BD718XX, n, val, __VA_ARGS__)

#define buck1_4_mV_to_regval(mV)	(((mV) - 700) / 10)

/*
 * BUCK5 voltage
 *   If D[7]=0,
 *	000 = 0.70 V
 *	001 = 0.80 V
 *	010 = 0.90 V
 *	011 = 1.00 V
 *	100 = 1.05 V
 *	101 = 1.10 V
 *	110 = 1.20 V
 *	111 = 1.35 V
 *   If D[7]=1,
 *	000 = 0.675 V
 *	001 = 0.775 V
 *	010 = 0.875 V
 *	011 = 0.975 V
 *	100 = 1.025 V
 *	101 = 1.075 V
 *	110 = 1.175 V
 *	111 = 1.325 V
*/
#define buck5_mV_to_regval(mV)		(((mV) <= 1000) ? (((mV) - 700) / 100) : \
					 ((mV <= 1050) ? 0x4 :		\
					  ((mV < 1350) ? (((mV) - 1050) / 100) + 5 : 0x7)))

#define buck6_mV_to_regval(mV)		(((mV) - 3000) / 100)

/*
 * BUCK7 voltage
 *	000 = 1.605 V
 *	001 = 1.695 V
 *	010 = 1.755 V
 *	011 = 1.800 V
 *	100 = 1.845 V
 *	101 = 1.905 V
 *	110 = 1.950 V
 *	111 = 1.995 V
 */
#define buck7_mV_to_regval(mV)		(((mV) <= 1695) ? (((mV) - 1605) / 90) : \
					 ((mV) <= 1755) ? 0x2 :		\
					 ((mV) <= 1845) ? ((mV) - 1755) / 45 + 2 : \
					 ((mV) <= 1905) ? ((mV) - 1845) / 60 + 4 : \
					 ((mV) - 1905) / 45 + 6)

#define buck8_mV_to_regval(mV)		(((mV) - 800) / 10)

#define VDD_SOC_0V8_VAL			buck1_4_mV_to_regval(900)
#define VDD_ARM_0V9_VAL			buck1_4_mV_to_regval(900)
#define VDD_DRAM_PU_0V9_VAL		buck5_mV_to_regval(900)
#define VDD_3V3_VAL			buck6_mV_to_regval(3300)
#define VDD_1V8_VAL			buck7_mV_to_regval(1800)
#define NVCC_DRAM_VAL			buck8_mV_to_regval(1350)

static const struct pmic_val pmic_vals[] = {
	/* decrease RESET key long push time from the default 10s to 10ms */
	BD71847_REG(PWRONCONFIG1, 0),
	/* unlock the PMIC regs */
	BD71847_REG(REGLOCK, BD718XX_REGLOCK_PWRSEQ),
	BD71847_REG(BUCK1_CTRL, 0, ~1),
	BD71847_REG(BUCK2_CTRL, 0, ~1),
	BD71847_REG(1ST_NODVS_BUCK_CTRL, 0, ~1),
	BD71847_REG(2ND_NODVS_BUCK_CTRL, 0, ~1),
	BD71847_REG(3RD_NODVS_BUCK_CTRL, 0, ~1),
	BD71847_REG(BUCK1_VOLT_RUN, VDD_SOC_0V8_VAL),
	BD71847_REG(BUCK2_VOLT_RUN, VDD_ARM_0V9_VAL),
	/* increase VDD_DRAM to 0.9v for 3Ghz DDR */
	BD71847_REG(1ST_NODVS_BUCK_VOLT, VDD_DRAM_PU_0V9_VAL),
	BD71847_REG(2ND_NODVS_BUCK_VOLT, VDD_3V3_VAL),
	BD71847_REG(3RD_NODVS_BUCK_VOLT, VDD_1V8_VAL),
	/* increase NVCC_DRAM_1V35 to 1.35v for DDR3L */
	BD71847_REG(4TH_NODVS_BUCK_VOLT, NVCC_DRAM_VAL),

	/* lock the PMIC regs */
	BD71847_REG(REGLOCK, BD718XX_REGLOCK_VREG, ~BD718XX_REGLOCK_VREG),
};

#define PMIC_I2C_BUS			0
#define PMIC_NAME			"BD71837"

int power_init_board(void)
{
	int ret;
	struct pmic *pmic;
	size_t i;

	ret = power_bd71837_init(PMIC_I2C_BUS);
	if (ret) {
		printf("power init failed: %d\n", ret);
		return ret;
	}

	pmic = pmic_get(PMIC_NAME);
	if (!pmic) {
		printf("PMIC %s not registered\n", PMIC_NAME);
		return -ENODEV;
	}

	ret = pmic_probe(pmic);
	if (ret)
		goto out;

	for (i = 0; ret == 0 && i < ARRAY_SIZE(pmic_vals); i++) {
		const struct pmic_val *r = &pmic_vals[i];

		ret = pmic_update_reg(pmic, r->addr, r->val, r->mask, r->name);
	}

 out:
	if (CONFIG_IS_ENABLED(BANNER_PRINT))
		printf("PMIC:  %s %s\n", PMIC_NAME, ret ? "FAILED" : "OK");
	else if (ret)
		printf("PMIC:  %s FAILED: %d\n", PMIC_NAME, ret);

	return 0;
}
