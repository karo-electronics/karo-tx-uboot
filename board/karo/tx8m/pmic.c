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
#if defined(CONFIG_POWER) || IS_ENABLED(CONFIG_DM_PMIC)
#include <power/pmic.h>
#endif
#if CONFIG_IS_ENABLED(DM_PMIC_BD71837) || defined(CONFIG_POWER_BD71837)
#include <power/bd71837.h>
#endif
#if CONFIG_IS_ENABLED(DM_PMIC_PCA9450) || defined(CONFIG_POWER_PCA9450)
#include <power/pca9450.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

struct pmic_val {
	u8 addr;
	u8 val;
	const char *name;
};

#if IS_ENABLED(CONFIG_DM_PMIC)
static inline int pmic_update_reg(struct udevice *dev, uint reg, uint value,
				  const char *reg_name)
{
	int ret;

	ret = pmic_reg_read(dev, reg);
	if (ret < 0) {
		printf("Failed to read register %s[0x%02x]: %d\n",
		       reg_name, reg, ret);
		return ret;
	}
	debug("Changing reg 0x%02x from 0x%02x to 0x%02x\n",
	      reg, ret, value);
	ret = pmic_reg_write(dev, reg, value);
	if (ret) {
		printf("Failed to write 0x%02x to register %s[0x%02x]: %d\n",
		       value, reg_name, reg, ret);
		return ret;
	}
	return ret;
}
#elif defined(CONFIG_POWER)
static inline int pmic_update_reg(struct pmic *p, uint reg, uint value,
				  const char *reg_name)
{
	int ret;
	u32 val;

	ret = pmic_reg_read(p, reg, &val);
	if (ret) {
		printf("Failed to read register %s[0x%02x]: %d\n",
		       reg_name, reg, ret);
		return ret;
	}
	debug("Changing reg %s[0x%02x] from 0x%02x to 0x%02x\n",
	      reg_name, reg, val, value);
	ret = pmic_reg_write(p, reg, value);
	if (ret) {
		printf("Failed to write 0x%02x to register %s[0x%02x]: %d\n",
		       value, reg_name, reg, ret);
		return ret;
	}
	return ret;
}
#endif

#define pmic_reg_write(p, r, v)	pmic_update_reg(p, r, v, #r)

#if defined(CONFIG_POWER_BD71837)

#define I2C_PMIC	0
#define PMIC_NAME	"BD71837"

int power_init_board(void)
{
	struct pmic *p;
	int ret;

	ret = power_bd71837_init(I2C_PMIC);
	if (ret) {
		printf("power init failed: %d\n", ret);
		return ret;
	}

	p = pmic_get(PMIC_NAME);
	if (!p) {
		printf("PMIC %s not registered\n", PMIC_NAME);
		return -ENODEV;
	}

	ret = pmic_probe(p);
	if (ret)
		goto out;

	/* decrease RESET key long push time from the default 10s to 10ms */
	ret = pmic_reg_write(p, BD718XX_PWRONCONFIG1, 0x0);
	if (ret)
		goto out;

	/* unlock the PMIC regs */
	ret = pmic_reg_write(p, BD718XX_REGLOCK, 0x1);
	if (ret)
		goto out;

	/* increase VDD_DRAM to 0.9v for 3Ghz DDR */
	ret = pmic_reg_write(p, BD718XX_1ST_NODVS_BUCK_VOLT, 0x2);
	if (ret)
		goto out;

	/* increase NVCC_DRAM_1V35 to 1.35v for DDR3L */
	ret = pmic_reg_write(p, BD718XX_4TH_NODVS_BUCK_VOLT, 0x37);
	if (ret)
		goto out;

	/* lock the PMIC regs */
	ret = pmic_reg_write(p, BD718XX_REGLOCK, 0x11);

 out:
	if (CONFIG_IS_ENABLED(BANNER_PRINT))
		printf("PMIC:  %s %s\n", PMIC_NAME, ret ? "FAILED" : "OK");
	else if (ret)
		printf("PMIC:  %s FAILED: %d\n", PMIC_NAME, ret);

	return 0;
}
#endif
