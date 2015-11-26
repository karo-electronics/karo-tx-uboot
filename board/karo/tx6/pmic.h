/*
 * Copyright (C) 2014 Lothar Waßmann <LW@KARO-electronics.de>
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

#ifdef CONFIG_RN5T567
#include "rn5t567.h"
#endif

struct pmic_regs {
	u8 addr;
	u8 val;
	u8 mask;
};

typedef int pmic_setup_func(uchar addr, struct pmic_regs *regs,
			size_t num_regs);

int ltc3676_pmic_setup(uchar addr, struct pmic_regs *regs, size_t num_regs);
int rn5t618_pmic_setup(uchar addr, struct pmic_regs *regs, size_t num_regs);
int rn5t567_pmic_setup(uchar addr, struct pmic_regs *regs, size_t num_regs);

int tx6_pmic_init(int addr, struct pmic_regs *regs, size_t num_regs);
