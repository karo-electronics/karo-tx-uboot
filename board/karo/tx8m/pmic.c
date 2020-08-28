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

static const struct pmic_val pmic_vals[] __maybe_unused = {
#if defined(CONFIG_POWER_PCA9450)

#define PCA9450_REG(n, val) { PCA9450_##n, val, #n, }

#define pca9450_mV_to_reg(mV)	(((mV) - 600) * 10 / 125)
#define pca9450_reg_to_mV(val)	(((val) * 125 / 10) + 600)

#define VDD_SOC_VAL	pca9450_mV_to_reg(950)
#define VDD_SOC_SLP_VAL	pca9450_mV_to_reg(850)
#define VDD_ARM_VAL	pca9450_mV_to_reg(950)
#define VDD_DRAM_VAL	pca9450_mV_to_reg(950)

	PCA9450_REG(BUCK123_DVS, 0x29),
	PCA9450_REG(BUCK1OUT_DVS0, VDD_SOC_VAL),
	PCA9450_REG(BUCK1OUT_DVS1, VDD_SOC_SLP_VAL),
	PCA9450_REG(BUCK2OUT_DVS0, VDD_ARM_VAL),
	PCA9450_REG(BUCK3OUT_DVS0, VDD_DRAM_VAL),
	PCA9450_REG(BUCK1CTRL, 0x59),
	PCA9450_REG(RESET_CTRL, 0xA1),
#elif defined(CONFIG_POWER_BD71837)
#endif
};

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
#elif defined(CONFIG_POWER_PCA9450)

#define I2C_PMIC	0
#define PMIC_NAME	"PCA9450"

int power_init_board(void)
{
	int ret;
	struct pmic *pmic;
	size_t i;

	ret = power_pca9450b_init(I2C_PMIC);
	if (ret) {
		printf("power init failed: %d\n", ret);
		return ret;
	}

	pmic = pmic_get(PMIC_NAME);
	if (!pmic) {
		printf("Could not get '%s' PMIC\n", PMIC_NAME);
		return -ENODEV;
	}

	ret = pmic_probe(pmic);
	if (ret) {
		printf("Failed to probe pmic %s: %d\n", PMIC_NAME, ret);
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(pmic_vals); i++) {
		const struct pmic_val *r = &pmic_vals[i];

		ret = pmic_update_reg(pmic, r->addr, r->val, r->name);
		if (ret) {
			printf("Failed to write PMIC reg 0x%02x: %d\n",
			       r->addr, ret);
			goto out;
		}
	}

 out:
	if (CONFIG_IS_ENABLED(BANNER_PRINT))
		printf("PMIC:  %s %s\n", PMIC_NAME, ret ? "FAILED" : "OK");
	else if (ret)
		printf("PMIC:  %s FAILED: %d\n", PMIC_NAME, ret);

	return ret;
}
#endif /* CONFIG_POWER_PCA9450 */

#ifdef CONFIG_KARO_QS8M
#define I2C_PMIC		0x34

static const struct {
	u8 reg;
	u8 val;
	u8 mask;
} qs8m_pmic_data[] = {
	{ 0x50, 0, },
	{ 0x5c, 0, },
};

int power_init_board(void)
{
	int ret;
	size_t i;

	ret = i2c_probe(I2C_PMIC);
	if (ret) {
		printf("Could not find PMIC @0x%02x: %d\n", I2C_PMIC, ret);
		return ret;
	}
	for (i = 0; i < ARRAY_SIZE(qs8m_pmic_data); i++) {
		printf("%s: setting reg 0x%02x to 0x%02x\n", __func__,
		       qs8m_pmic_data[i].reg, qs8m_pmic_data[i].val);
		ret = i2c_write(I2C_PMIC, qs8m_pmic_data[i].reg, 1,
				(uchar *)&qs8m_pmic_data[i].val, 1);
		if (ret)
			printf("Failed to write 0x%02x to PMIC @0x%02x reg 0x%02x: %d\n",
			       qs8m_pmic_data[i].val, I2C_PMIC,
			       qs8m_pmic_data[i].reg, ret);
	}
	return ret;
}
#endif
