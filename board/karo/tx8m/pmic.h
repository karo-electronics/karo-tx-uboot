/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */
#define DEBUG

struct pmic_val {
	const char *name;
	u8 addr;
	u8 val;
	u8 mask;
};

#define PMIC_REG_VAL(pfx, n, val, ...) { #n, pfx##_##n, val, __VA_ARGS__ }

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
	if (ret == 0 && val != value) {
		debug("Changing PMIC reg %s(0x%02x) from 0x%02x to 0x%02x\n",
		      reg_name, reg, val, value);
	} else if (ret == 0 && val == value) {
		debug("Leaving PMIC reg %s(0x%02x) at 0x%02x\n",
		      reg_name, reg, value);
	} else {
		debug("Setting PMIC reg %s(0x%02x) to 0x%02x\n",
		      reg_name, reg, value);
	}

	ret = pmic_reg_write(dev, reg, value);
	if (ret) {
		printf("Failed to write 0x%02x to register %s[0x%02x]: %d\n",
		       value, reg_name, reg, ret);
		return ret;
	}
	return ret;
}
#elif defined(CONFIG_KARO_QS8M)
static inline int pmic_update_reg(int i2c_addr, uint reg, u8 value,
				  u8 mask, const char *reg_name)
{
	int ret;
	u8 val;

	if (mask && value & mask) {
		printf("PMIC register %s[%02x] value %02x does not fit mask %02x\n",
		       reg_name, reg, value, mask);
		return -EINVAL;
	}

	ret = i2c_read(i2c_addr, reg, 1, &val, 1);
	if (ret) {
		printf("Failed to read register %s[0x%02x]: %d\n",
		       reg_name, reg, ret);
		return ret;
	} else if (mask) {
		value |= val & mask;
	}

	if (ret == 0 && val != value) {
		debug("Changing PMIC reg %s(0x%02x) from 0x%02x to 0x%02x\n",
		      reg_name, reg, val, value);
	} else if (ret == 0 && val == value) {
		debug("Leaving PMIC reg %s(0x%02x) at 0x%02x\n",
		      reg_name, reg, value);
	} else {
		debug("Setting PMIC reg %s(0x%02x) to 0x%02x\n",
		      reg_name, reg, value);
	}

	ret = i2c_write(i2c_addr, reg, 1, &value, 1);
	if (ret) {
		printf("Failed to write 0x%02x to register %s[0x%02x]: %d\n",
		       value, reg_name, reg, ret);
		return ret;
	}
	return ret;
}
#elif defined(CONFIG_POWER)
static inline int pmic_update_reg(struct pmic *p, uint reg, u32 value,
				  u32 mask, const char *reg_name)
{
	int ret;
	u32 val;

	if (mask && value & mask) {
		printf("PMIC register %s[%02x] value %02x does not fit mask %02x\n",
		       reg_name, reg, value, mask);
		return -EINVAL;
	}

	ret = pmic_reg_read(p, reg, &val);
	if (ret) {
		printf("Failed to read register %s[0x%02x]: %d\n",
		       reg_name, reg, ret);
		return ret;
	} else if (mask) {
		value |= val & mask;
	}

	if (ret == 0 && val != value) {
		debug("Changing PMIC reg %s(0x%02x) from 0x%02x to 0x%02x\n",
		      reg_name, reg, val, value);
	} else if (ret == 0 && val == value) {
		debug("Leaving PMIC reg %s(0x%02x) at 0x%02x\n",
		      reg_name, reg, value);
	} else {
		debug("Setting PMIC reg %s(0x%02x) to 0x%02x\n",
		      reg_name, reg, value);
	}

	ret = pmic_reg_write(p, reg, value);
	if (ret) {
		printf("Failed to write 0x%02x to register %s[0x%02x]: %d\n",
		       value, reg_name, reg, ret);
		return ret;
	}
	return ret;
}
#else
#error No PMIC access method available
#endif
