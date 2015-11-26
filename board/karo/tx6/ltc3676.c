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

#include <common.h>
#include <i2c.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>

#include "../common/karo.h"
#include "pmic.h"

#define LTC3676_BUCK1		0x01
#define LTC3676_BUCK2		0x02
#define LTC3676_BUCK3		0x03
#define LTC3676_BUCK4		0x04
#define LTC3676_DVB1A		0x0A
#define LTC3676_DVB1B		0x0B
#define LTC3676_DVB2A		0x0C
#define LTC3676_DVB2B		0x0D
#define LTC3676_DVB3A		0x0E
#define LTC3676_DVB3B		0x0F
#define LTC3676_DVB4A		0x10
#define LTC3676_DVB4B		0x11
#define LTC3676_MSKPG		0x13
#define LTC3676_CLIRQ		0x1f

#define LTC3676_BUCK_DVDT_FAST	(1 << 0)
#define LTC3676_BUCK_KEEP_ALIVE	(1 << 1)
#define LTC3676_BUCK_CLK_RATE_LOW (1 << 2)
#define LTC3676_BUCK_PHASE_SEL	(1 << 3)
#define LTC3676_BUCK_ENABLE_300	(1 << 4)
#define LTC3676_BUCK_PULSE_SKIP	(0 << 5)
#define LTC3676_BUCK_BURST_MODE	(1 << 5)
#define LTC3676_BUCK_CONTINUOUS	(2 << 5)
#define LTC3676_BUCK_ENABLE	(1 << 7)

#define LTC3676_PGOOD_MASK	(1 << 5)

#define LTC3676_MSKPG_BUCK1	(1 << 0)
#define LTC3676_MSKPG_BUCK2	(1 << 1)
#define LTC3676_MSKPG_BUCK3	(1 << 2)
#define LTC3676_MSKPG_BUCK4	(1 << 3)
#define LTC3676_MSKPG_LDO2	(1 << 5)
#define LTC3676_MSKPG_LDO3	(1 << 6)
#define LTC3676_MSKPG_LDO4	(1 << 7)

#define VDD_IO_VAL		mV_to_regval(vout_to_vref(3300, 5))
#define VDD_IO_VAL_LP		mV_to_regval(vout_to_vref(3100, 5))
#define VDD_IO_VAL_2		mV_to_regval(vout_to_vref(3300, 5_2))
#define VDD_IO_VAL_2_LP		mV_to_regval(vout_to_vref(3100, 5_2))
#define VDD_SOC_VAL		mV_to_regval(vout_to_vref(1425, 6))
#define VDD_SOC_VAL_LP		mV_to_regval(vout_to_vref(900, 6))
#define VDD_DDR_VAL		mV_to_regval(vout_to_vref(1500, 7))
#define VDD_DDR_VAL_LP		mV_to_regval(vout_to_vref(1500, 7))
#define VDD_CORE_VAL		mV_to_regval(vout_to_vref(1425, 8))
#define VDD_CORE_VAL_LP		mV_to_regval(vout_to_vref(900, 8))

/* LDO1 */
#define R1_1			470
#define R2_1			150
/* LDO4 */
#define R1_4			470
#define R2_4			150
/* Buck1 */
#define R1_5			390
#define R2_5			110
#define R1_5_2			470
#define R2_5_2			150
/* Buck2 (SOC) */
#define R1_6			150
#define R2_6			180
/* Buck3 (DDR) */
#define R1_7			150
#define R2_7			140
/* Buck4 (CORE) */
#define R1_8			150
#define R2_8			180

/* calculate voltages in 10mV */
#define R1(idx)			R1_##idx
#define R2(idx)			R2_##idx

#define v2r(v,n,m)		DIV_ROUND(((((v) < (n)) ? (n) : (v)) - (n)), (m))
#define r2v(r,n,m)		(((r) * (m) + (n)) / 10)

#define vout_to_vref(vout, idx)	((vout) * R2(idx) / (R1(idx) + R2(idx)))
#define vref_to_vout(vref, idx)	DIV_ROUND_UP((vref) * (R1(idx) + R2(idx)), R2(idx))

#define mV_to_regval(mV)	v2r((mV) * 10, 4125, 125)
#define regval_to_mV(r)		r2v(r, 4125, 125)

static struct pmic_regs ltc3676_regs[] = {
	{ LTC3676_MSKPG, ~LTC3676_MSKPG_BUCK1, },
	{ LTC3676_DVB2B, VDD_SOC_VAL_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB3B, VDD_DDR_VAL_LP, ~0x3f, },
	{ LTC3676_DVB4B, VDD_CORE_VAL_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB2A, VDD_SOC_VAL, ~0x3f, },
	{ LTC3676_DVB3A, VDD_DDR_VAL, ~0x3f, },
	{ LTC3676_DVB4A, VDD_CORE_VAL, ~0x3f, },
	{ LTC3676_BUCK1, LTC3676_BUCK_BURST_MODE | LTC3676_BUCK_CLK_RATE_LOW, },
	{ LTC3676_BUCK2, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_BUCK3, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_BUCK4, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_CLIRQ, 0, }, /* clear interrupt status */
};

static struct pmic_regs ltc3676_regs_1[] = {
	{ LTC3676_DVB1B, VDD_IO_VAL_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB1A, VDD_IO_VAL, ~0x3f, },
};

static struct pmic_regs ltc3676_regs_2[] = {
	{ LTC3676_DVB1B, VDD_IO_VAL_2_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB1A, VDD_IO_VAL_2, ~0x3f, },
};

static int tx6_rev_2(void)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank5_regs *fuse = (void *)ocotp->bank[5].fuse_regs;
	u32 pad_settings = readl(&fuse->pad_settings);

	debug("Fuse pad_settings @ %p = %02x\n",
		&fuse->pad_settings, pad_settings);
	return pad_settings & 1;
}

static int ltc3676_setup_regs(uchar slave_addr, struct pmic_regs *r,
			size_t count)
{
	int ret;
	int i;

	for (i = 0; i < count; i++, r++) {
#ifdef DEBUG
		unsigned char value;

		ret = i2c_read(slave_addr, r->addr, 1, &value, 1);
		if (ret) {
			printf("%s: failed to read PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
		if ((value & ~r->mask) != r->val) {
			printf("Changing PMIC reg %02x from %02x to %02x\n",
				r->addr, value, r->val);
		}
#endif
		ret = i2c_write(slave_addr, r->addr, 1, &r->val, 1);
		if (ret) {
			printf("%s: failed to write PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
#ifdef DEBUG
		ret = i2c_read(slave_addr, r->addr, 1, &value, 1);
		if (ret) {
			printf("%s: failed to read PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
		if (value != r->val) {
			printf("Failed to set PMIC reg %02x to %02x; actual value: %02x\n",
				r->addr, r->val, value);
		}
#endif
	}
	return 0;
}

int ltc3676_pmic_setup(uchar slave_addr, struct pmic_regs *regs,
			size_t count)
{
	int ret;
	unsigned char value;

	ret = i2c_read(slave_addr, 0x11, 1, &value, 1);
	if (ret) {
		printf("%s: i2c_read error: %d\n", __func__, ret);
		return ret;
	}

	ret = ltc3676_setup_regs(slave_addr, ltc3676_regs,
				ARRAY_SIZE(ltc3676_regs));
	if (ret)
		return ret;

	ret = i2c_read(slave_addr, LTC3676_DVB4A, 1, &value, 1);
	if (ret == 0) {
		printf("VDDCORE set to %umV\n",
			vref_to_vout(regval_to_mV(value), 8));
	} else {
		printf("Failed to read VDDCORE register setting\n");
	}

	ret = i2c_read(slave_addr, LTC3676_DVB2A, 1, &value, 1);
	if (ret == 0) {
		printf("VDDSOC  set to %umV\n",
			vref_to_vout(regval_to_mV(value), 6));
	} else {
		printf("Failed to read VDDSOC register setting\n");
	}

	if (tx6_rev_2()) {
		ret = ltc3676_setup_regs(slave_addr, ltc3676_regs_2,
				ARRAY_SIZE(ltc3676_regs_2));

		ret = i2c_read(slave_addr, LTC3676_DVB1A, 1, &value, 1);
		if (ret == 0) {
			printf("VDDIO   set to %umV\n",
				vref_to_vout(regval_to_mV(value), 5_2));
		} else {
			printf("Failed to read VDDIO register setting\n");
		}
	} else {
		ret = ltc3676_setup_regs(slave_addr, ltc3676_regs_1,
				ARRAY_SIZE(ltc3676_regs_1));
	}
	return ret;
}
