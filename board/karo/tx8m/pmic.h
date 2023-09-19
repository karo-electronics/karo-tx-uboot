/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#if CONFIG_IS_ENABLED(DM_PMIC)
int power_init_board(void);

struct pmic_val {
	const char *name;
	u8 addr;
	u8 val;
	u8 mask;
};

#define PMIC_REG_VAL(pfx, n, val, ...) { #n, pfx##_##n, val, __VA_ARGS__ }

static inline int pmic_update_reg(struct udevice *pmic, uint reg, uint value,
				  uint mask, const char *reg_name)
{
	int ret;
	u32 val;

	if (mask && value & mask) {
		printf("PMIC register %s[%02x] value %02x does not fit mask %02x\n",
		       reg_name, reg, value, mask);
		return -EINVAL;
	}

	ret = pmic_reg_read(pmic, reg);
	if (ret < 0) {
		printf("Failed to read register %s[0x%02x]: %d\n",
		       reg_name, reg, ret);
		return ret;
	}
	val = ret;

	if (mask) {
		value |= val & mask;
	}

	if (val != value) {
		debug("Changing PMIC reg %s(0x%02x) from 0x%02x to 0x%02x\n",
		      reg_name, reg, val, value);
	} else {
		debug("Leaving PMIC reg %s(0x%02x) at 0x%02x\n",
		      reg_name, reg, value);
	}

	ret = pmic_reg_write(pmic, reg, value);
	if (ret) {
		printf("Failed to write 0x%02x to register %s[0x%02x]: %d\n",
		       value, reg_name, reg, ret);
		return ret;
	}
	return 0;
}
#else
static inline int power_init_board(void)
{
	return 0;
}
#endif
