/*
 * Copyright (C) 2015 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <common.h>
#include <errno.h>
#include <i2c.h>

#include "pmic.h"

#ifdef CONFIG_SYS_I2C
static struct {
	uchar addr;
	pmic_setup_func *init;
	const char *name;
} i2c_addrs[] = {
#ifdef CONFIG_LTC3676
	{ 0x3c, ltc3676_pmic_setup, "LTC3676", },
#endif
#ifdef CONFIG_RN5T618
	{ 0x32, rn5t618_pmic_setup, "RN5T618", },
#endif
#ifdef CONFIG_RN5T567
	{ 0x33, rn5t567_pmic_setup, "RN5T567", },
#endif
};

int tx6_pmic_init(int i2c_addr, struct pmic_regs *regs, size_t num_regs)
{
	int ret = -ENODEV;
	int i;
	const char *pmic = "N/A";

	printf("PMIC: ");

	for (i = 0; i < ARRAY_SIZE(i2c_addrs); i++) {

		if (i2c_addrs[i].addr == i2c_addr) {
			pmic = i2c_addrs[i].name;
			break;
		}
	}
	printf("%s\n", pmic);

	debug("Probing for I2C dev 0x%02x\n", i2c_addr);
	ret = i2c_probe(i2c_addr);
	if (ret == 0) {
		debug("Initializing PMIC at I2C addr 0x%02x\n", i2c_addr);
		ret = i2c_addrs[i].init(i2c_addr, regs, num_regs);
	}
	return ret;
}
#else
int tx6_pmic_init(int i2c_addr, struct pmic_regs *regs, size_t num_regs)
{
	printf("PMIC: N/A\n");
	return 0;
}
#endif
