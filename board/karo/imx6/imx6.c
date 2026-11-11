// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Ka-Ro electronics GmbH
 *
 * Ka-Ro TX6S/TX6U/TX6Q (i.MX6 S/DL/Q/QP) and TXUL (i.MX6UL/ULL)
 */
#include <console.h>
#include <dm.h>
#include <env.h>
#include <fdt_support.h>
#include <fsl_wdog.h>
#include <fuse.h>
#include <init.h>
#include <mtd_node.h>
#include <thermal.h>
#include <time.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/sys_proto.h>
#include <jffs2/load_kernel.h>
#include <linux/bitops.h>
#include <linux/libfdt.h>

#include "../common/karo.h"
#include "karo_pmic.h"

#include <asm/arch/clock.h>
#include <asm/arch/crm_regs.h>
#include <linux/delay.h>

#if (defined(CONFIG_MX6S) || defined(CONFIG_MX6DL) || \
	defined(CONFIG_MX6Q)) && IS_ENABLED(CONFIG_VIDEO_IPUV3)
#include <asm/mach-imx/video.h>
/* Private to drivers/video/imx/mxcfb.h; latch pixel data on falling edge. */
#define FB_SYNC_CLK_LAT_FALL	0x40000000
#endif

DECLARE_GLOBAL_DATA_PTR;

/* PMU_REG_3P0[7]: 0 = USB_OTG1_VBUS, 1 = USB_OTG2_VBUS */
#define PMU_REG_3P0_VBUS_SEL_OTG2 BIT(7)

int dram_init(void)
{
	gd->ram_size = imx_ddr_size();
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM;
	gd->bd->bi_dram[0].size  = imx_ddr_size();
	return 0;
}

/* Read by karo_env_set_uboot_vars() in common/env.c to set serial# */
void get_board_serial(struct tag_serialnr *serialnr)
{
	u32 low = 0, high = 0;

	fuse_read(0, 1, &low);
	fuse_read(0, 2, &high);
	serialnr->low  = low;
	serialnr->high = high;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
#if defined(CONFIG_KARO_TX6S_8034)
	printf("Board: Ka-Ro TX6S-8034\n");
#elif defined(CONFIG_KARO_TX6S_8134)
	printf("Board: Ka-Ro TX6S-8134\n");
#elif defined(CONFIG_KARO_TX6S_8035)
	printf("Board: Ka-Ro TX6S-8035\n");
#elif defined(CONFIG_KARO_TX6S_8135)
	printf("Board: Ka-Ro TX6S-8135\n");
#elif defined(CONFIG_KARO_TX6U_8030)
	printf("Board: Ka-Ro TX6U-8030\n");
#elif defined(CONFIG_KARO_TX6U_8130)
	printf("Board: Ka-Ro TX6U-8130\n");
#elif defined(CONFIG_KARO_TX6U_8033)
	printf("Board: Ka-Ro TX6U-8033\n");
#elif defined(CONFIG_KARO_TX6U_8133)
	printf("Board: Ka-Ro TX6U-8133\n");
#elif defined(CONFIG_KARO_TX6Q_1030)
	printf("Board: Ka-Ro TX6Q-1030\n");
#elif defined(CONFIG_KARO_TX6Q_1130)
	printf("Board: Ka-Ro TX6Q-1130\n");
#elif defined(CONFIG_KARO_TX6Q_1036)
	printf("Board: Ka-Ro TX6Q-1036\n");
#elif defined(CONFIG_KARO_TX6Q_8037)
	printf("Board: Ka-Ro TX6Q-8037\n");
#elif defined(CONFIG_KARO_TX6Q_8137)
	printf("Board: Ka-Ro TX6Q-8137\n");
#elif defined(CONFIG_KARO_TXUL_8013)
	printf("Board: Ka-Ro TXUL-8013\n");
#elif defined(CONFIG_KARO_TXUL_5011)
	printf("Board: Ka-Ro TXUL-5011\n");
#elif defined(CONFIG_KARO_TXUL_5010)
	printf("Board: Ka-Ro TXUL-5010\n");
#else
#error Unsupported module variant
#endif

	return 0;
}
#endif

static bool karo_wdog_reset(void)
{
	struct watchdog_regs *wdog = (struct watchdog_regs *)WDOG1_BASE_ADDR;

	return readw(&wdog->wrsr) & WRSR_TOUT;
}

static void imx6_print_cpu_temp(void)
{
	struct udevice *dev;
	int temp;

	if (!IS_ENABLED(CONFIG_IMX_THERMAL))
		return;
	if (uclass_first_device_err(UCLASS_THERMAL, &dev))
		return;
	if (!thermal_get_temp(dev, &temp))
		printf("CPU temperature: %d C\n", temp);
}

#if defined(CONFIG_MX6UL) || defined(CONFIG_MX6ULL)
#define TXUL_JTAG_IN_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_SPEED_LOW | PAD_CTL_PUS_47K_UP)
/* PKE without PUE: TDO keeper, IMX6ULIEC Table 5 */
#define TXUL_JTAG_OUT_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_PKE | PAD_CTL_SPEED_LOW | \
					     PAD_CTL_DSE_60ohm | PAD_CTL_SRE_SLOW)

static const iomux_v3_cfg_t txul_jtag_pads[] = {
	MX6_PAD_JTAG_MOD__SJC_MOD | TXUL_JTAG_IN_PAD_CTRL,
	MX6_PAD_JTAG_TCK__SJC_TCK | TXUL_JTAG_IN_PAD_CTRL,
	MX6_PAD_JTAG_TRST_B__SJC_TRSTB | TXUL_JTAG_IN_PAD_CTRL,
	MX6_PAD_JTAG_TDI__SJC_TDI | TXUL_JTAG_IN_PAD_CTRL,
	MX6_PAD_JTAG_TDO__SJC_TDO | TXUL_JTAG_OUT_PAD_CTRL,
	MX6_PAD_JTAG_TMS__SJC_TMS | TXUL_JTAG_IN_PAD_CTRL,
};
#endif /* CONFIG_MX6UL || CONFIG_MX6ULL */

#if IS_ENABLED(CONFIG_VIDEO)
/* common/splash.c draws any logo; this only powers the panel */
static void imx6_lcd_enable(void)
{
	struct gpio_desc *pwr, *rst, *bl;
	struct udevice *vdev;

	/* Probe first: never power an undriven panel */
	if (uclass_first_device_err(UCLASS_VIDEO, &vdev))
		return;

	/* Hogs are named after their line-name property */
	if (gpio_hog_lookup_name("lcd-power", &pwr) ||
	    gpio_hog_lookup_name("lcd-reset", &rst) ||
	    gpio_hog_lookup_name("lcd-backlight", &bl)) {
		printf("Failed to look up the LCD control GPIOs\n");
		return;
	}

	/* Order and delays of the legacy lcd_enable() */
	dm_gpio_set_value(pwr, 1);
	udelay(100);
	dm_gpio_set_value(rst, 1);
	mdelay(300);
	dm_gpio_set_value(bl, 1);
}
#else
static inline void imx6_lcd_enable(void) {}
#endif

#if (defined(CONFIG_MX6S) || defined(CONFIG_MX6DL) || \
	defined(CONFIG_MX6Q)) && IS_ENABLED(CONFIG_VIDEO_IPUV3)
/* Compile-time display selection; one panel per variant family. */
#if defined(CONFIG_KARO_TX6S_8134) || defined(CONFIG_KARO_TX6S_8135) || \
	defined(CONFIG_KARO_TX6U_8130) || defined(CONFIG_KARO_TX6U_8133) || \
	defined(CONFIG_KARO_TX6Q_1130) || defined(CONFIG_KARO_TX6Q_8137)
#define TX6_LVDS	1
/* 18-bit LDB packs into DI[17:0]: same DC map as parallel RGB666. */
#define TX6_PIX_FMT	IPU_PIX_FMT_RGB666
/* FB_SYNC_EXT: DI clock comes from the LDB serialiser. */
#define TX6_SYNC	(FB_SYNC_EXT | FB_SYNC_CLK_LAT_FALL)
#else
#define TX6_LVDS	0
#define TX6_PIX_FMT	IPU_PIX_FMT_RGB24
/* Drive on falling edge: MB7 U9 DS90C385A strobes rising */
#define TX6_SYNC	0
#endif

#if TX6_LVDS
/* Kernel panel-simple.c edt_et1010g0dsa_mode: ETML1010G0DKA 1280x800 */
static const struct fb_videomode tx6_panel = {
	.name		= "ETML1010G0DKA",
	.refresh	= 60,
	.xres		= 1280,
	.yres		= 800,
	.pixclock	= KHZ2PICOS(71000),
	.left_margin	= 55,
	.hsync_len	= 80,
	.right_margin	= 25,
	.upper_margin	= 16,
	.vsync_len	= 2,
	.lower_margin	= 5,
	.sync		= TX6_SYNC,
	.vmode		= FB_VMODE_NONINTERLACED,
};
#else
static const struct fb_videomode tx6_panel = {
	.name		= "ET0700",
	.refresh	= 60,
	.xres		= 800,
	.yres		= 480,
	.pixclock	= KHZ2PICOS(33260),
	.left_margin	= 88,
	.hsync_len	= 128,
	.right_margin	= 40,
	.upper_margin	= 33,
	.vsync_len	= 2,
	.lower_margin	= 10,
	.sync		= TX6_SYNC,
	.vmode		= FB_VMODE_NONINTERLACED,
};
#endif

#if TX6_LVDS
/* PLL5 to 24*(41+10/24)/2 = 497 MHz = 7 * 71 MHz exactly. */
static void tx6_setup_pll5(void)
{
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	ulong start;

	/* IMX6DQRM 18.5.1.5.3: power down before changing */
	writel(BM_ANADIG_PLL_VIDEO_POWERDOWN, &ccm->analog_pll_video_set);
	writel(BM_ANADIG_PLL_VIDEO_BYPASS |
	       BM_ANADIG_PLL_VIDEO_DIV_SELECT |
	       BM_ANADIG_PLL_VIDEO_POST_DIV_SELECT,
	       &ccm->analog_pll_video_clr);

	/* POST_DIV_SELECT 1 = /2 */
	writel(BF_ANADIG_PLL_VIDEO_DIV_SELECT(41) |
	       BF_ANADIG_PLL_VIDEO_POST_DIV_SELECT(1),
	       &ccm->analog_pll_video_set);
	writel(BF_ANADIG_PLL_VIDEO_NUM_A(10),
	       &ccm->analog_pll_video_num);
	writel(BF_ANADIG_PLL_VIDEO_DENOM_B(24),
	       &ccm->analog_pll_video_denom);

	/* Power up after the change */
	writel(BM_ANADIG_PLL_VIDEO_POWERDOWN, &ccm->analog_pll_video_clr);

	/* Bounded wait: a stuck PLL must not hang the boot. */
	start = get_timer(0);
	while (!(readl(&ccm->analog_pll_video) & BM_ANADIG_PLL_VIDEO_LOCK)) {
		if (get_timer(start) > 10) {
			printf("PLL5 lock timeout\n");
			break;
		}
	}

	writel(BM_ANADIG_PLL_VIDEO_BYPASS, &ccm->analog_pll_video_clr);
	writel(BM_ANADIG_PLL_VIDEO_ENABLE, &ccm->analog_pll_video_set);
}

/* No DM driver for LDB: program clocks and mux by hand. */
static void tx6_setup_ldb(void)
{
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	struct iomuxc *iomux = (struct iomuxc *)IOMUXC_BASE_ADDR;
	u32 gpr3;

	/* Mux before PLL5 power-up: the ERR009219 sequence disables PLL5. */
	select_ldb_di_clock_source(MXC_PLL5_CLK);
	tx6_setup_pll5();
	setbits_le32(&ccm->cscmr2, MXC_CCM_CSCMR2_LDB_DI0_IPU_DIV);
	clrsetbits_le32(&ccm->chsccdr,
			MXC_CCM_CHSCCDR_IPU1_DI0_CLK_SEL_MASK <<
			MXC_CCM_CHSCCDR_IPU1_DI0_CLK_SEL_OFFSET,
			CHSCCDR_CLK_SEL_LDB_DI0 <<
			MXC_CCM_CHSCCDR_IPU1_DI0_CLK_SEL_OFFSET);
	setbits_le32(&ccm->CCGR3, MXC_CCM_CCGR3_LDB_DI0_MASK);

	/* Kernel imx6qdl-tx6-lvds.dtsi: channel 0 on DI0, SPWG, 18 bit */
	/* DI drives VSYNC active low: no FB_SYNC_VERT_HIGH_ACT */
	writel(IOMUXC_GPR2_DATA_WIDTH_CH0_18BIT |
	       IOMUXC_GPR2_DATA_WIDTH_CH1_18BIT |
	       IOMUXC_GPR2_BIT_MAPPING_CH0_SPWG |
	       IOMUXC_GPR2_BIT_MAPPING_CH1_SPWG |
	       IOMUXC_GPR2_DI0_VS_POLARITY_ACTIVE_LOW |
	       IOMUXC_GPR2_LVDS_CH0_MODE_ENABLED_DI0,
	       &iomux->gpr[2]);

	gpr3 = readl(&iomux->gpr[3]);
	gpr3 &= ~(IOMUXC_GPR3_LVDS1_MUX_CTL_MASK |
		  IOMUXC_GPR3_LVDS0_MUX_CTL_MASK);
	writel(gpr3, &iomux->gpr[3]);
}
#endif

/* Called by the IPUv3 driver probe; supplies the single compiled-in panel. */
int ipu_displays_init(void)
{
	int ret;

#if TX6_LVDS
	tx6_setup_ldb();
#endif
	ret = ipuv3_fb_init(&tx6_panel, 0, TX6_PIX_FMT);

	if (ret) {
		printf("Can't init display: %d\n", ret);
		return ret;
	}
	/* No DDC/EDID on MB7: report interface and mode, not a panel. */
	printf("Display: %s %ux%u\n",
	       TX6_LVDS ? "LVDS" : "RGB",
	       tx6_panel.xres, tx6_panel.yres);
	return 0;
}
#endif /* (MX6S || MX6DL || MX6Q) && VIDEO_IPUV3 */

int board_init(void)
{
	if (ctrlc())
		printf("<CTRL-C> detected; safeboot enabled\n");
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

#if (defined(CONFIG_MX6S) || defined(CONFIG_MX6DL) || \
	defined(CONFIG_MX6Q)) && IS_ENABLED(CONFIG_VIDEO_IPUV3)
	enable_ipu_clock();
#endif

#if defined(CONFIG_MX6S) || defined(CONFIG_MX6DL) || defined(CONFIG_MX6Q)
	struct gpio_desc reset_out;
	int ret;

	/* #RESET_OUT to the baseboard: released by pull-up */
	ret = dm_gpio_lookup_name("gpio7_12", &reset_out);
	if (ret) {
		printf("Failed to lookup #RESET_OUT GPIO: %d\n", ret);
		return ret;
	}
	ret = dm_gpio_request(&reset_out, "#RESET_OUT");
	if (ret) {
		printf("Failed to request #RESET_OUT GPIO: %d\n", ret);
		return ret;
	}

	imx_iomux_v3_setup_pad(MX6_PAD_GPIO_17__GPIO7_IO12 |
			       MUX_PAD_CTRL(PAD_CTL_PUS_100K_UP |
					    PAD_CTL_SPEED_MED |
					    PAD_CTL_DSE_40ohm |
					    PAD_CTL_SRE_FAST));
#endif

	/* The DCD sets up GPMI (NAND modules) and ENET (S/DL/Q/QP) clocks */
#if defined(CONFIG_MX6UL) || defined(CONFIG_MX6ULL)
	struct anatop_regs *anatop = (struct anatop_regs *)ANATOP_BASE_ADDR;

	if (IS_ENABLED(CONFIG_FEC_MXC)) {
		int ret = enable_fec_anatop_clock(0, ENET_50MHZ);

		if (ret)
			printf("FEC anatop clock: %d\n", ret);
		enable_enet_clk(1);
	}

	/*
	 * ERR010661: while both VBUS inputs are live, the one selected
	 * by PMU_REG_3P0[VBUS_SEL] stops dropping when its cable is
	 * unplugged, so the workaround is to select the host port.
	 * OTG1 is the device port and OTG2 the host port on every TXUL;
	 * the reset value selects OTG1.  With a single VBUS present the
	 * SoC selects it automatically, so LDO_3P0 keeps its input.
	 */
	writel(PMU_REG_3P0_VBUS_SEL_OTG2, &anatop->reg_3p0_set);
#endif
	return 0;
}

int power_init_board(void)
{
	int ret;

	if (!IS_ENABLED(CONFIG_PMIC_RN5T567))
		return 0;

	if (had_ctrlc()) {
		printf("<CTRL-C> detected; skipping PMIC setup\n");
		return 0;
	}
	if (karo_wdog_reset()) {
		printf("WDOG RESET detected; Skipping PMIC setup\n");
		return 0;
	}

	ret = karo_pmic_setup();
	if (ret)
		printf("Failed to setup PMIC voltages: %d\n", ret);

	return 0;
}

/* Legacy tx6ul.c/tx6qdl.c set_cpu_clock(): PLL1 = 24 MHz * m / 2, ARM = PLL1 / d */
static int imx6_set_arm_clk(unsigned long freq_mhz)
{
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	const unsigned long ref = 24;
	unsigned long d, m = 0, div = 0;
	int timeout = 1000;

	for (d = DIV_ROUND_UP(648, freq_mhz); d <= 8; d++) {
		m = freq_mhz * 2 * d / ref;
		if (m > 108)
			return -EINVAL;
		if (m >= 54 && ref * m / 2 / d == freq_mhz) {
			div = d;
			break;
		}
	}
	if (!div)
		return -EINVAL;

	setbits_le32(&ccm->ccsr, MXC_CCM_CCSR_PLL1_SW_CLK_SEL);
	clrsetbits_le32(&ccm->analog_pll_sys, BM_ANADIG_PLL_SYS_DIV_SELECT, m);
	writel(div - 1, &ccm->cacrr);
	while (!(readl(&ccm->analog_pll_sys) & BM_ANADIG_PLL_SYS_LOCK)) {
		if (--timeout == 0) {
			printf("ARM PLL failed to lock\n");
			return -ETIMEDOUT;
		}
		udelay(10);
	}
	clrbits_le32(&ccm->ccsr, MXC_CCM_CCSR_PLL1_SW_CLK_SEL);
	return 0;
}

static void imx6_set_cpu_clock(void)
{
	unsigned long cpu_clk = env_get_ulong("cpu_clk", 10, 0);

	if (cpu_clk == 0 || cpu_clk == mxc_get_clock(MXC_ARM_CLK) / 1000000)
		return;

	if (had_ctrlc() || karo_wdog_reset()) {
		printf("%s detected; skipping cpu clock change\n",
		       karo_wdog_reset() ? "WDOG RESET" : "<CTRL-C>");
		return;
	}
	if (imx6_set_arm_clk(cpu_clk) == 0) {
		cpu_clk = mxc_get_clock(MXC_ARM_CLK);
		printf("CPU clock set to %lu.%03lu MHz\n",
		       cpu_clk / 1000000, cpu_clk / 1000 % 1000);
	} else {
		printf("Error: Failed to set CPU clock to %lu MHz\n", cpu_clk);
	}
}

int board_late_init(void)
{
	int ret;
	const char *fdt_file;

	/* Register the SoC modes so 'bmode usb' can reach the ROM loader. */
	if (IS_ENABLED(CONFIG_CMD_BMODE))
		add_board_boot_modes(NULL);

	/* Report the running control DT and its baseboard source */
	if (IS_ENABLED(CONFIG_KARO_DTB_SELECT))
		karo_dtb_report();

	karo_env_cleanup();
	imx6_print_cpu_temp();
	imx6_set_cpu_clock();

	if (had_ctrlc()) {
		env_set("safeboot", "1");
	} else if (karo_wdog_reset()) {
		env_set("wdreset", "1");
	} else if (!IS_ENABLED(CONFIG_KARO_UBOOT_MFG)) {
		/* Fallback as tx93.c:174-178; MFG flashes a blank medium */
		fdt_file = env_get("fdt_file");
		ret = karo_load_fdt(fdt_file ?: CONFIG_DEFAULT_FDT_FILE);
		if (ret)
			printf("Failed to load FDT from '%s': %d\n",
			       fdt_file ?: CONFIG_DEFAULT_FDT_FILE, ret);
	}
	karo_set_ethaddr(0);

	imx6_lcd_enable();

#if defined(CONFIG_MX6UL) || defined(CONFIG_MX6ULL)
	/* Pads muxed as JTAG unless jtag_enable is explicitly n. */
	if (env_get_yesno("jtag_enable") != 0)
		imx_iomux_v3_setup_multiple_pads(txul_jtag_pads, ARRAY_SIZE(txul_jtag_pads));
#endif

	clear_ctrlc();
	return 0;
}

#if defined(CONFIG_OF_BOARD_SETUP)
/* Env mtdparts overrides MTDPARTS_DEFAULT; erase stale envs after relayout. */
#define IMX6_NAND_COMPAT	"fsl,imx6q-gpmi-nand"

#if IS_ENABLED(CONFIG_KARO_IMX6_NAND)
static const struct node_info imx6_nand_nodes[] = {
	{ IMX6_NAND_COMPAT, MTD_DEV_TYPE_NAND, },
};
#endif

static void imx6_disable_alias(void *blob, const char *alias)
{
	int node = fdt_path_offset(blob, alias);
	int ret;

	if (node < 0)
		return;
	ret = fdt_status_disabled(blob, node);
	if (ret)
		printf("Failed to disable '%s': %s\n", alias, fdt_strerror(ret));
}

static const char *const imx6_touchpanels[] = {
	"tsc2007",
	"edt-ft5x06",
	"egalax_ts",
};

static void imx6_fixup_touchpanel(void *blob)
{
	const char *model = env_get("touchpanel");
	unsigned int i;

	if (!model)
		return;

	for (i = 0; i < ARRAY_SIZE(imx6_touchpanels); i++) {
		const char *tp = imx6_touchpanels[i];
		bool keep = false;

		if (model) {
			if (!strcmp(model, tp))
				keep = true;
			while (!keep && (tp = strchr(tp, '-')) != NULL) {
				tp++;
				if (*tp != '\0' && !strcmp(model, tp))
					keep = true;
			}
		}
		if (!keep)
			imx6_disable_alias(blob, imx6_touchpanels[i]);
	}
}

static void imx6_fixup_otg_vbus(void *blob)
{
	const char *otg_mode = env_get("otg_mode");

	if (otg_mode && !strcmp(otg_mode, "host"))
		imx6_disable_alias(blob, "can1");
	else
		imx6_disable_alias(blob, "usbotgvbus");
}

#if IS_ENABLED(CONFIG_KARO_IMX6_NAND)
/* ofpart expects explicit cells on the partitioned node. */
static void imx6_fixup_mtd_cells(void *blob)
{
	int node = fdt_node_offset_by_compatible(blob, -1, IMX6_NAND_COMPAT);

	if (node < 0)
		return;
	fdt_setprop_u32(blob, node, "#address-cells", 1);
	fdt_setprop_u32(blob, node, "#size-cells", 1);
}
#endif

int ft_board_setup(void *blob, struct bd_info *bd)
{
#if IS_ENABLED(CONFIG_KARO_IMX6_NAND)
	fdt_fixup_mtdparts(blob, imx6_nand_nodes, ARRAY_SIZE(imx6_nand_nodes));
	imx6_fixup_mtd_cells(blob);
#endif
	imx6_fixup_touchpanel(blob);
	imx6_fixup_otg_vbus(blob);

	return ft_karo_common_setup(blob, bd);
}
#endif
