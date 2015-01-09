/*
 * (C) Copyright 2007
 * Sascha Hauer, Pengutronix
 *
 * (C) Copyright 2009 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <div64.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/regs-ocotp.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/imx-common/boot_mode.h>
#include <asm/imx-common/dma.h>
#include <stdbool.h>
#ifdef CONFIG_VIDEO_IPUV3
#include <ipu.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_MX6_TEMPERATURE_MIN
#define TEMPERATURE_MIN			CONFIG_MX6_TEMPERATURE_MIN
#else
#define TEMPERATURE_MIN			(-40)
#endif
#ifdef CONFIG_MX6_TEMPERATURE_HOT
#define TEMPERATURE_HOT			CONFIG_MX6_TEMPERATURE_HOT
#else
#define TEMPERATURE_HOT			80
#endif
#ifdef CONFIG_MX6_TEMPERATURE_MAX
#define TEMPERATURE_MAX			CONFIG_MX6_TEMPERATURE_MAX
#else
#define TEMPERATURE_MAX			125
#endif
#define TEMP_AVG_COUNT			5
#define TEMP_WARN_THRESHOLD		5

#define __data	__attribute__((section(".data")))

struct scu_regs {
	u32	ctrl;
	u32	config;
	u32	status;
	u32	invalidate;
	u32	fpga_rev;
};

#ifdef CONFIG_HW_WATCHDOG
#define wdog_base	((void *)WDOG1_BASE_ADDR)
#define WDOG_WCR	0x00
#define WCR_WDE		(1 << 2)
#define WDOG_WSR	0x02

void hw_watchdog_reset(void)
{
	if (readw(wdog_base + WDOG_WCR) & WCR_WDE) {
		static u16 __data toggle = 0xaaaa;
		static int __data first = 1;

		if (first) {
			printf("Watchdog active\n");
			first = 0;
		}
		writew(toggle, wdog_base + WDOG_WSR);
		toggle ^= 0xffff;
	}
}
#endif

u32 get_cpu_rev(void)
{
	struct anatop_regs *anatop = (struct anatop_regs *)ANATOP_BASE_ADDR;
	u32 reg = readl(&anatop->digprog_sololite);
	u32 type = ((reg >> 16) & 0xff);

	if (type != MXC_CPU_MX6SL) {
		reg = readl(&anatop->digprog);
		type = ((reg >> 16) & 0xff);
		if (type == MXC_CPU_MX6DL) {
			struct scu_regs *scu = (struct scu_regs *)SCU_BASE_ADDR;
			u32 cfg = readl(&scu->config) & 3;

			if (!cfg)
				type = MXC_CPU_MX6SOLO;
		}
	}
	reg &= 0xff;		/* mx6 silicon revision */
	return (type << 12) | (reg + 0x10);
}

#ifdef CONFIG_REVISION_TAG
u32 __weak get_board_rev(void)
{
	u32 cpurev = get_cpu_rev();
	u32 type = ((cpurev >> 12) & 0xff);
	if (type == MXC_CPU_MX6SOLO)
		cpurev = (MXC_CPU_MX6DL) << 12 | (cpurev & 0xFFF);

	return cpurev;
}
#endif

void init_aips(void)
{
	struct aipstz_regs *aips1, *aips2;

	aips1 = (struct aipstz_regs *)AIPS1_BASE_ADDR;
	aips2 = (struct aipstz_regs *)AIPS2_BASE_ADDR;

	/*
	 * Set all MPROTx to be non-bufferable, trusted for R/W,
	 * not forced to user-mode.
	 */
	writel(0x77777777, &aips1->mprot0);
	writel(0x77777777, &aips1->mprot1);
	writel(0x77777777, &aips2->mprot0);
	writel(0x77777777, &aips2->mprot1);

	/*
	 * Set all OPACRx to be non-bufferable, not require
	 * supervisor privilege level for access,allow for
	 * write access and untrusted master access.
	 */
	writel(0x00000000, &aips1->opacr0);
	writel(0x00000000, &aips1->opacr1);
	writel(0x00000000, &aips1->opacr2);
	writel(0x00000000, &aips1->opacr3);
	writel(0x00000000, &aips1->opacr4);
	writel(0x00000000, &aips2->opacr0);
	writel(0x00000000, &aips2->opacr1);
	writel(0x00000000, &aips2->opacr2);
	writel(0x00000000, &aips2->opacr3);
	writel(0x00000000, &aips2->opacr4);
}

/*
 * Set the VDDSOC
 *
 * Mask out the REG_CORE[22:18] bits (REG2_TRIG) and set
 * them to the specified millivolt level.
 * Possible values are from 0.725V to 1.450V in steps of
 * 0.025V (25mV).
 */
static void set_vddsoc(u32 mv)
{
	struct anatop_regs *anatop = (struct anatop_regs *)ANATOP_BASE_ADDR;
	u32 val, reg = readl(&anatop->reg_core);

	if (mv < 725)
		val = 0x00;	/* Power gated off */
	else if (mv > 1450)
		val = 0x1F;	/* Power FET switched full on. No regulation */
	else
		val = (mv - 700) / 25;

	/*
	 * Mask out the REG_CORE[22:18] bits (REG2_TRIG)
	 * and set them to the calculated value (0.7V + val * 0.25V)
	 */
	reg = (reg & ~(0x1F << 18)) | (val << 18);
	writel(reg, &anatop->reg_core);
}

static u32 __data thermal_calib;

#define FACTOR0				10000000
#define FACTOR1				15976
#define FACTOR2				4297157

int raw_to_celsius(unsigned int raw, unsigned int raw_25c, unsigned int raw_hot,
		unsigned int hot_temp)
{
	int temperature;

	if (raw_hot != 0 && hot_temp != 0) {
		unsigned int raw_n40c, ratio;

		ratio = ((raw_25c - raw_hot) * 100) / (hot_temp - 25);
		raw_n40c = raw_25c + (13 * ratio) / 20;
		if (raw <= raw_n40c)
			temperature = (raw_n40c - raw) * 100 / ratio - 40;
		else
			temperature = TEMPERATURE_MIN;
	} else {
		u64 temp64 = FACTOR0;
		unsigned int c1, c2;
		/*
		 * Derived from linear interpolation:
		 * slope = 0.4297157 - (0.0015976 * 25C fuse)
		 * slope = (FACTOR2 - FACTOR1 * n1) / FACTOR0
		 * (Nmeas - n1) / (Tmeas - t1) = slope
		 * We want to reduce this down to the minimum computation necessary
		 * for each temperature read.  Also, we want Tmeas in millicelsius
		 * and we don't want to lose precision from integer division. So...
		 * Tmeas = (Nmeas - n1) / slope + t1
		 * milli_Tmeas = 1000 * (Nmeas - n1) / slope + 1000 * t1
		 * milli_Tmeas = -1000 * (n1 - Nmeas) / slope + 1000 * t1
		 * Let constant c1 = (-1000 / slope)
		 * milli_Tmeas = (n1 - Nmeas) * c1 + 1000 * t1
		 * Let constant c2 = n1 *c1 + 1000 * t1
		 * milli_Tmeas = c2 - Nmeas * c1
		 */
		temp64 *= 1000;
		do_div(temp64, FACTOR1 * raw_25c - FACTOR2);
		c1 = temp64;
		c2 = raw_25c * c1 + 1000 * 25;
		temperature = (c2 - raw * c1) / 1000;
	}
	return temperature;
}

int read_cpu_temperature(void)
{
	unsigned int reg, tmp, i;
	unsigned int raw_25c, raw_hot, hot_temp;
	int temperature;
	struct anatop_regs *const anatop = (void *)ANATOP_BASE_ADDR;
	struct mx6_ocotp_regs *const ocotp_regs = (void *)OCOTP_BASE_ADDR;

	if (!thermal_calib) {
		ocotp_clk_enable();
		writel(1, &ocotp_regs->hw_ocotp_read_ctrl);
		thermal_calib = readl(&ocotp_regs->hw_ocotp_ana1);
		writel(0, &ocotp_regs->hw_ocotp_read_ctrl);
		ocotp_clk_disable();
	}

	if (thermal_calib == 0 || thermal_calib == 0xffffffff)
		return TEMPERATURE_MIN;

	/* Fuse data layout:
	 * [31:20] sensor value @ 25C
	 * [19:8] sensor value of hot
	 * [7:0] hot temperature value */
	raw_25c = thermal_calib >> 20;
	raw_hot = (thermal_calib & 0xfff00) >> 8;
	hot_temp = thermal_calib & 0xff;

	/* now we only using single measure, every time we measure
	 * the temperature, we will power on/off the anadig module
	 */
	writel(BM_ANADIG_TEMPSENSE0_POWER_DOWN, &anatop->tempsense0_clr);
	writel(BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF, &anatop->ana_misc0_set);

	/* write measure freq */
	writel(327, &anatop->tempsense1);
	writel(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP, &anatop->tempsense0_clr);
	writel(BM_ANADIG_TEMPSENSE0_FINISHED, &anatop->tempsense0_clr);
	writel(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP, &anatop->tempsense0_set);

	/* average the temperature value over multiple readings */
	for (i = 0; i < TEMP_AVG_COUNT; i++) {
		static int failed;
		int limit = 100;

		while ((readl(&anatop->tempsense0) &
				BM_ANADIG_TEMPSENSE0_FINISHED) == 0) {
			udelay(10000);
			if (--limit < 0)
				break;
		}
		if ((readl(&anatop->tempsense0) &
				BM_ANADIG_TEMPSENSE0_FINISHED) == 0) {
			if (!failed) {
				printf("Failed to read temp sensor\n");
				failed = 1;
			}
			return 0;
		}
		failed = 0;
		reg = (readl(&anatop->tempsense0) &
			BM_ANADIG_TEMPSENSE0_TEMP_VALUE) >>
			BP_ANADIG_TEMPSENSE0_TEMP_VALUE;
		if (i == 0)
			tmp = reg;
		else
			tmp = (tmp * i + reg) / (i + 1);
		writel(BM_ANADIG_TEMPSENSE0_FINISHED,
			&anatop->tempsense0_clr);
	}

	temperature = raw_to_celsius(tmp, raw_25c, raw_hot, hot_temp);

	/* power down anatop thermal sensor */
	writel(BM_ANADIG_TEMPSENSE0_POWER_DOWN, &anatop->tempsense0_set);
	writel(BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF, &anatop->ana_misc0_clr);

	return temperature;
}

int check_cpu_temperature(int boot)
{
	static int __data max_temp;
	int boot_limit = getenv_ulong("max_boot_temp", 10, TEMPERATURE_HOT);
	int tmp = read_cpu_temperature();
	bool first = true;

	if (tmp < TEMPERATURE_MIN || tmp > TEMPERATURE_MAX) {
		printf("Temperature:   can't get valid data!\n");
		return tmp;
	}

	if (!boot) {
		if (tmp > boot_limit) {
			printf("CPU is %d C, too hot, resetting...\n", tmp);
			udelay(100000);
			reset_cpu(0);
		}
		if (tmp > max_temp) {
			if (tmp > boot_limit - TEMP_WARN_THRESHOLD)
				printf("WARNING: CPU temperature %d C\n", tmp);
			max_temp = tmp;
		}
	} else {
		printf("Temperature:   %d C, calibration data 0x%x\n",
			tmp, thermal_calib);
		while (tmp >= boot_limit) {
			if (first) {
				printf("CPU is %d C, too hot to boot, waiting...\n",
					tmp);
				first = false;
			}
			if (ctrlc())
				break;
			udelay(50000);
			tmp = read_cpu_temperature();
			if (tmp > boot_limit - TEMP_WARN_THRESHOLD && tmp != max_temp)
				printf("WARNING: CPU temperature %d C\n", tmp);
			max_temp = tmp;
		}
	}
	return tmp;
}

static void imx_set_wdog_powerdown(bool enable)
{
	struct wdog_regs *wdog1 = (struct wdog_regs *)WDOG1_BASE_ADDR;
	struct wdog_regs *wdog2 = (struct wdog_regs *)WDOG2_BASE_ADDR;

	/* Write to the PDE (Power Down Enable) bit */
	writew(enable, &wdog1->wmcr);
	writew(enable, &wdog2->wmcr);
}

#ifdef CONFIG_ARCH_CPU_INIT
int arch_cpu_init(void)
{
	init_aips();

	set_vddsoc(1200);	/* Set VDDSOC to 1.2V */

	imx_set_wdog_powerdown(false); /* Disable PDE bit of WMCR register */

#ifdef CONFIG_VIDEO_IPUV3
	gd->arch.ipu_hw_rev = IPUV3_HW_REV_IPUV3H;
#endif
#ifdef  CONFIG_APBH_DMA
	/* Timer is required for Initializing APBH DMA */
	timer_init();
	mxs_dma_init();
#endif
	return 0;
}
#endif

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in start.S */
	dcache_enable();
}
#endif

#if defined(CONFIG_FEC_MXC)
void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank *bank = &ocotp->bank[4];
	struct fuse_bank4_regs *fuse =
			(struct fuse_bank4_regs *)bank->fuse_regs;

	u32 value = readl(&fuse->mac_addr_high);
	mac[0] = (value >> 8);
	mac[1] = value;

	value = readl(&fuse->mac_addr_low);
	mac[2] = value >> 24;
	mac[3] = value >> 16;
	mac[4] = value >> 8;
	mac[5] = value;
}
#endif

void boot_mode_apply(unsigned cfg_val)
{
	unsigned reg;
	struct src *psrc = (struct src *)SRC_BASE_ADDR;
	writel(cfg_val, &psrc->gpr9);
	reg = readl(&psrc->gpr10);
	if (cfg_val)
		reg |= 1 << 28;
	else
		reg &= ~(1 << 28);
	writel(reg, &psrc->gpr10);
}
/*
 * cfg_val will be used for
 * Boot_cfg4[7:0]:Boot_cfg3[7:0]:Boot_cfg2[7:0]:Boot_cfg1[7:0]
 * After reset, if GPR10[28] is 1, ROM will copy GPR9[25:0]
 * to SBMR1, which will determine the boot device.
 */
const struct boot_mode soc_boot_modes[] = {
	{"normal",	MAKE_CFGVAL(0x00, 0x00, 0x00, 0x00)},
	/* reserved value should start rom usb */
	{"usb",		MAKE_CFGVAL(0x01, 0x00, 0x00, 0x00)},
	{"sata",	MAKE_CFGVAL(0x20, 0x00, 0x00, 0x00)},
	{"escpi1:0",	MAKE_CFGVAL(0x30, 0x00, 0x00, 0x08)},
	{"escpi1:1",	MAKE_CFGVAL(0x30, 0x00, 0x00, 0x18)},
	{"escpi1:2",	MAKE_CFGVAL(0x30, 0x00, 0x00, 0x28)},
	{"escpi1:3",	MAKE_CFGVAL(0x30, 0x00, 0x00, 0x38)},
	/* 4 bit bus width */
	{"esdhc1",	MAKE_CFGVAL(0x40, 0x20, 0x00, 0x00)},
	{"esdhc2",	MAKE_CFGVAL(0x40, 0x28, 0x00, 0x00)},
	{"esdhc3",	MAKE_CFGVAL(0x40, 0x30, 0x00, 0x00)},
	{"esdhc4",	MAKE_CFGVAL(0x40, 0x38, 0x00, 0x00)},
	{NULL,		0},
};

void s_init(void)
{
}
