/*
 * ddr3.c
 *
 * AM33XX emif4 configuration file
 *
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * based on emif4.c
 * Copyright (C) 2011, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <asm/sizes.h>
#include <asm/arch/cpu.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/ddr3_defs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/clock.h>
#include <asm/io.h>

/* AM335X EMIF Register values */
#define	VTP_CTRL_READY		(0x1 << 5)
#define VTP_CTRL_ENABLE		(0x1 << 6)
#define VTP_CTRL_LOCK_EN	(0x1 << 4)
#define VTP_CTRL_START_EN	(0x1 << 0)

/*
 * DDR3 force values.  These are board dependent
 */
/*
 * Invert clock adds an additional half cycle delay on the command
 * interface.  The additional half cycle, is usually meant to enable
 * leveling in the situation that DQS is later than CK on the board.  It
 * also helps provide some additional margin for leveling.
 *
 * For the EVM this is helping us with additional room for the write
 * leveling.  Since the dqs delays are very small.
 */
#define INVERT_CLOCK		0

/*
 * This represents the initial value for the leveling process.  The
 * value is a ratio - so 0x100 represents one cycle.  The real delay
 * is determined through the leveling process.
 *
 * During the leveling process, 0x20 is subtracted from the value, so
 * we have added that to the value we want to set.  We also set the
 * values such that byte3 completes leveling after byte2 and byte1
 * after byte0.
 */
#define WR_DQS_RATIO_0		0x20
#define WR_DQS_RATIO_1		0x20

/*
 * This represents the initial value for the leveling process.  The
 * value is a ratio - so 0x100 represents one cycle.  The real delay
 * is determined through the leveling process.
 *
 * During the leveling process, 0x20 is subtracted from the value, so
 * we have added that to the value we want to set.  We also set the
 * values such that byte3 completes leveling after byte2 and byte1
 * after byte0.
 */
#define RD_GATE_RATIO_0		0x20
#define RD_GATE_RATIO_1		0x20

/*
 * CMD_SLAVE_RATIO determines where is the command placed with respect
 * to the clock edge.  This is a ratio, implying 0x100 is one cycle.
 * Ideally the command is centered so - this should be half cycle
 * delay (0x80).  But if invert clock is in use, an additional half
 * cycle must be added
 */
#define CMD_SLAVE_FROM_INV_CLOCK(i) (((i) == 0) ? 0x80 : 0x100)
#define CMD_SLAVE_RATIO		CMD_SLAVE_FROM_INV_CLOCK(INVERT_CLOCK)

/*
 * EMIF Paramters.  Refer the EMIF register documentation and the
 * memory datasheet for details
 */
/* For 303 MHz m_clk 3.3ns */
#define EMIF_TIM1    0x0668A3DB
/*
 * 000 0011 0011 0100 01010 001111 011 011
 *
 *28-25   reg_t_rp      3 - 13ns
 *24-21   reg_t_rcd     3 - 13ns
 *20-17   reg_t_wr      4 - 16ns
 *16-12   reg_t_ras    10 - 36ns
 *11-6    reg_t_rc     15 - 52ns
 *5-3     reg_t_rrd     3
 *2-0     reg_t_wtr     3
 */

#define EMIF_TIM2    0x2A04011A
/*
 * 0 010 101 000000100 0000000100 011 010
 *
 *30-28   reg_t_xp     2 - 3nCK
 *27-25   reg_t_odt    5 - 6nCK
 *24-16   reg_t_xsnr   4 - 5nCK
 *15-6    reg_t_xsrd   4 - 5nCK
 *5-3     reg_t_rtp    3 - 4nCK
 *2-0     reg_t_cke    2 - 3nCK
 */

#define EMIF_TIM3    0x001F8309
/*
 * 00000000 000 111111 00 000110000 1001
 *
 *23-21   reg_t_ckesr      0  - LPDDR2
 *20-15   reg_zq_zqcs      63 - 64nCK
 *14-13   reg_t_tdqsckmax  0  - LPDDR2
 *12-4    reg_t_rfc        48 - 161ns
 *3-0     reg_t_ras_max    9
 */

#define EMIF_SDREF   0x20000C1A
/*
 * 0 0 1 0 0 000 00000000 C1A
 *
 *31    reg_initref_dis   0 Initialization
 *30    Reserved          0
 *29    reg_srt           1 extended temp.
 *28    reg_asr           0 manual Self Refresh
 *27    Reserved          0
 *26-24 reg_pasr          0
 *23-16 Reserved          0
 *15-0  reg_refresh_rate  C1A
 */

#define EMIF_SDCFG   0x62A44AB2 /* 0x62A45032 */
/*
 * 011 00 010 1 01 0 01 00 01 0010 101 011 0 010
 *
 *31-29 reg_sdram_type      3 - DDR3
 *28-27 reg_ibank_pos       0
 *26-24 reg_ddr_term        2 - RZQ/2
 *23    reg_ddr2_ddqs       1 - differential DQS
 *22-21 reg_dyn_odt         1 - Dynamic ODT RZQ/4
 *20    reg_ddr_disable_dll 0 - enable DLL
 *19-18 reg_sdram_drive     1 - drive strength RZQ/7
 *17-16 reg_cwl             0 - CAS write latency 5
 *15-14 reg_narrow_mode     1 - 16-bit data bus width
 *13-10 reg_cl              2 - CAS latency of 5
 *9-7   reg_rowsize         5 - 14 row bits
 *6-4   reg_ibank           3 - 8 banks
 *3     reg_ebank           0 - 1 chip select
 *2-0   reg_pagesize        2 - 10 column bits
 */

#define EMIF_PHYCFG		0x0000010B

#define	PHY_RANK_DELAY		0x01
#define DDR_IOCTRL_VALUE	0x18B

DECLARE_GLOBAL_DATA_PTR;

#define PHY_DLL_LOCK_DIFF 0x0f

struct ddr_regs *ddrregs = (struct ddr_regs *)DDR_PHY_BASE_ADDR;
struct vtp_reg *vtpreg = (struct vtp_reg *)VTP0_CTRL_ADDR;
struct ddr_ctrl *ddrctrl = (struct ddr_ctrl *)DDR_CTRL_ADDR;

int dram_init(void)
{
	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				CONFIG_MAX_RAM_BANK_SIZE);
	return 0;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size = gd->ram_size;
}

#ifdef CONFIG_SPL_BUILD
/**
 * Base address for EMIF instances
 */
static struct emif_regs *emif_reg = {
				(struct emif_regs *)EMIF4_0_CFG_BASE};

/**
 * Base address for DDR instance
 */
static struct ddr_regs *ddr_reg = (struct ddr_regs *)DDR_PHY_BASE_ADDR;

/**
 * Base address for ddr io control instances
 */
static struct ddr_cmdtctrl *ioctrl_reg = {
			(struct ddr_cmdtctrl *)DDR_CONTROL_BASE_ADDR};

static void data_macro_config(void)
{
	writel((WR_DQS_RATIO_0 << 10) | (WR_DQS_RATIO_0 << 0),
		&ddr_reg->dt0.wiratio0);
	writel((WR_DQS_RATIO_1 << 10) | (WR_DQS_RATIO_1 << 0),
		&ddr_reg->dt1.wiratio0);
	writel((RD_GATE_RATIO_0 << 10) | (RD_GATE_RATIO_0 << 0),
		&ddr_reg->dt0.giratio0);
	writel((RD_GATE_RATIO_1 << 10) | (RD_GATE_RATIO_1 << 0),
		&ddr_reg->dt1.giratio0);

	writel(PHY_DLL_LOCK_DIFF, &ddr_reg->dt0.dldiff0);
	writel(PHY_DLL_LOCK_DIFF, &ddr_reg->dt1.dldiff0);
}

static void cmd_macro_config(void)
{
	writel(PHY_DLL_LOCK_DIFF, &ddr_reg->cm0dldiff);
	writel(PHY_DLL_LOCK_DIFF, &ddr_reg->cm1dldiff);
	writel(PHY_DLL_LOCK_DIFF, &ddr_reg->cm2dldiff);

	writel(INVERT_CLOCK, &ddr_reg->cm0iclkout);
	writel(INVERT_CLOCK, &ddr_reg->cm1iclkout);
	writel(INVERT_CLOCK, &ddr_reg->cm2iclkout);
}

static void config_vtp(void)
{
	writel(readl(&vtpreg->vtp0ctrlreg) | VTP_CTRL_ENABLE,
			&vtpreg->vtp0ctrlreg);
	writel(readl(&vtpreg->vtp0ctrlreg) & ~VTP_CTRL_START_EN,
			&vtpreg->vtp0ctrlreg);
	writel(readl(&vtpreg->vtp0ctrlreg) | VTP_CTRL_START_EN,
			&vtpreg->vtp0ctrlreg);

	/* Poll for READY */
	while ((readl(&vtpreg->vtp0ctrlreg) & VTP_CTRL_READY) !=
			VTP_CTRL_READY)
		;
}

static void config_emif_ddr3(void)
{
	/* Program EMIF0 CFG Registers */
	writel(EMIF_PHYCFG, &emif_reg->ddrphycr);
	writel(EMIF_PHYCFG, &emif_reg->ddrphycsr);

	writel(EMIF_TIM1, &emif_reg->sdrtim1);
	writel(EMIF_TIM1, &emif_reg->sdrtim1sr);
	writel(EMIF_TIM2, &emif_reg->sdrtim2);
	writel(EMIF_TIM2, &emif_reg->sdrtim2sr);
	writel(EMIF_TIM3, &emif_reg->sdrtim3);
	writel(EMIF_TIM3, &emif_reg->sdrtim3sr);

	writel(EMIF_SDCFG, &emif_reg->sdrcr);
	writel(EMIF_SDCFG, &emif_reg->sdrcr2);
	writel(0x00004650, &emif_reg->sdrrcr);
	writel(0x00004650, &emif_reg->sdrrcsr);

	udelay(50);

	writel(EMIF_SDREF, &emif_reg->sdrrcr);
	writel(EMIF_SDREF, &emif_reg->sdrrcsr);
}

void config_ddr(void)
{
	enable_emif_clocks();

	config_vtp();

	cmd_macro_config();

	data_macro_config();

	writel(PHY_RANK_DELAY, &ddrregs->dt0.rdelays0);
	writel(PHY_RANK_DELAY, &ddrregs->dt1.rdelays0);

	writel(DDR_IOCTRL_VALUE, &ioctrl_reg->cm0ioctl);
	writel(DDR_IOCTRL_VALUE, &ioctrl_reg->cm1ioctl);
	writel(DDR_IOCTRL_VALUE, &ioctrl_reg->cm2ioctl);

	writel(DDR_IOCTRL_VALUE, &ioctrl_reg->dt0ioctl);
	writel(DDR_IOCTRL_VALUE, &ioctrl_reg->dt1ioctl);

	writel(readl(&ddrctrl->ddrioctrl) & 0xefffffff, &ddrctrl->ddrioctrl);
	writel(readl(&ddrctrl->ddrckectrl) | 0x00000001, &ddrctrl->ddrckectrl);

	config_emif_ddr3();
}
#endif
