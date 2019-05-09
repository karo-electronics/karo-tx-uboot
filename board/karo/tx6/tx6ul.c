/*
 * Copyright (C) 2015 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 *
 */
#include <common.h>
#include <errno.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <lcd.h>
#include <netdev.h>
#include <mmc.h>
#include <fsl_esdhc.h>
#include <video_fb.h>
#include <ipu.h>
#include <mxcfb.h>
#include <i2c.h>
#include <linux/fb.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/clock.h>
#include <asm/arch/hab.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/sys_proto.h>

#include "../common/karo.h"
#include "pmic.h"

#define __data __attribute__((section(".data")))

#define TX6UL_FEC_RST_GPIO		IMX_GPIO_NR(5, 6)
#define TX6UL_FEC_PWR_GPIO		IMX_GPIO_NR(5, 7)
#define TX6UL_FEC_INT_GPIO		IMX_GPIO_NR(5, 5)

#define TX6UL_FEC2_RST_GPIO		IMX_GPIO_NR(4, 28)
#define TX6UL_FEC2_INT_GPIO		IMX_GPIO_NR(4, 27)

#define TX6UL_LED_GPIO			IMX_GPIO_NR(5, 9)

#define TX6UL_LCD_PWR_GPIO		IMX_GPIO_NR(5, 4)
#define TX6UL_LCD_RST_GPIO		IMX_GPIO_NR(3, 4)
#define TX6UL_LCD_BACKLIGHT_GPIO	IMX_GPIO_NR(4, 16)

#ifdef CONFIG_SYS_I2C_SOFT
#define TX6UL_I2C1_SCL_GPIO		CONFIG_SOFT_I2C_GPIO_SCL
#define TX6UL_I2C1_SDA_GPIO		CONFIG_SOFT_I2C_GPIO_SDA
#endif

#define TX6UL_SD1_CD_GPIO		IMX_GPIO_NR(4, 14)

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

DECLARE_GLOBAL_DATA_PTR;

#define MUX_CFG_SION			IOMUX_PAD(0, 0, IOMUX_CONFIG_SION, 0, 0, 0)

char __uboot_img_end[0] __attribute__((section(".__uboot_img_end")));
#ifdef CONFIG_SECURE_BOOT
char __csf_data[0] __attribute__((section(".__csf_data")));
#endif

#define TX6UL_DEFAULT_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_PUS_100K_UP |	\
					PAD_CTL_SPEED_MED |		\
					PAD_CTL_DSE_40ohm |		\
					PAD_CTL_SRE_FAST)
#define TX6UL_I2C_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_PUS_22K_UP |	\
					PAD_CTL_ODE |			\
					PAD_CTL_HYS |			\
					PAD_CTL_SPEED_LOW |		\
					PAD_CTL_DSE_34ohm |		\
					PAD_CTL_SRE_FAST)
#define TX6UL_ENET_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_SPEED_HIGH |	\
					PAD_CTL_DSE_120ohm |		\
					PAD_CTL_PUS_100K_UP |		\
					PAD_CTL_SRE_FAST)
#define TX6UL_GPIO_OUT_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_SPEED_LOW |	\
					PAD_CTL_DSE_60ohm |		\
					PAD_CTL_SRE_SLOW)
#define TX6UL_GPIO_IN_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_SPEED_LOW |	\
					PAD_CTL_PUS_47K_UP)


static const iomux_v3_cfg_t const tx6ul_pads[] = {
	/* UART pads */
#if CONFIG_MXC_UART_BASE == UART1_BASE
	MX6_PAD_UART1_TX_DATA__UART1_DCE_TX | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_UART1_RX_DATA__UART1_DCE_RX | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_UART1_RTS_B__UART1_DCE_RTS | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_UART1_CTS_B__UART1_DCE_CTS | TX6UL_DEFAULT_PAD_CTRL,
#endif
#if CONFIG_MXC_UART_BASE == UART2_BASE
	MX6_PAD_UART2_TX_DATA__UART2_DCE_TX | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_UART2_RX_DATA__UART2_DCE_RX | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_UART3_RX_DATA__UART2_DCE_RTS | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_UART3_TX_DATA__UART2_DCE_CTS | TX6UL_DEFAULT_PAD_CTRL,
#endif
#if CONFIG_MXC_UART_BASE == UART5_BASE
	MX6_PAD_GPIO1_IO04__UART5_DCE_TX | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_GPIO1_IO05__UART5_DCE_RX | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_GPIO1_IO08__UART5_DCE_RTS | TX6UL_DEFAULT_PAD_CTRL,
	MX6_PAD_GPIO1_IO09__UART5_DCE_CTS | TX6UL_DEFAULT_PAD_CTRL,
#endif
	/* FEC PHY GPIO functions */
	MX6_PAD_SNVS_TAMPER7__GPIO5_IO07 | TX6UL_GPIO_OUT_PAD_CTRL, /* PHY POWER */
	MX6_PAD_SNVS_TAMPER6__GPIO5_IO06 | TX6UL_GPIO_OUT_PAD_CTRL, /* PHY RESET */
	MX6_PAD_SNVS_TAMPER5__GPIO5_IO05 | TX6UL_GPIO_IN_PAD_CTRL, /* PHY INT */
};

static const iomux_v3_cfg_t const tx6ul_enet1_pads[] = {
	/* FEC functions */
	MX6_PAD_GPIO1_IO07__ENET1_MDC | MUX_PAD_CTRL(PAD_CTL_DSE_120ohm |
						     PAD_CTL_SPEED_LOW),
	MX6_PAD_GPIO1_IO06__ENET1_MDIO | MUX_PAD_CTRL(PAD_CTL_PUS_100K_UP |
						      PAD_CTL_DSE_120ohm |
						      PAD_CTL_SPEED_LOW),
	MX6_PAD_ENET1_TX_CLK__ENET1_REF_CLK1 | MUX_PAD_CTRL(PAD_CTL_SPEED_LOW |
						     PAD_CTL_DSE_80ohm |
						     PAD_CTL_SRE_SLOW),

	MX6_PAD_ENET1_RX_ER__ENET1_RX_ER | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET1_RX_EN__ENET1_RX_EN | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET1_RX_DATA1__ENET1_RDATA01 | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET1_RX_DATA0__ENET1_RDATA00 | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET1_TX_EN__ENET1_TX_EN | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET1_TX_DATA1__ENET1_TDATA01 | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET1_TX_DATA0__ENET1_TDATA00 | TX6UL_ENET_PAD_CTRL,
};

static const iomux_v3_cfg_t const tx6ul_enet2_pads[] = {
	MX6_PAD_ENET2_TX_CLK__ENET2_REF_CLK2 | MUX_PAD_CTRL(PAD_CTL_SPEED_LOW |
							    PAD_CTL_DSE_80ohm |
							    PAD_CTL_SRE_SLOW),
	MX6_PAD_ENET2_RX_ER__ENET2_RX_ER | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET2_RX_EN__ENET2_RX_EN | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET2_RX_DATA1__ENET2_RDATA01 | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET2_RX_DATA0__ENET2_RDATA00 | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET2_TX_EN__ENET2_TX_EN | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET2_TX_DATA1__ENET2_TDATA01 | TX6UL_ENET_PAD_CTRL,
	MX6_PAD_ENET2_TX_DATA0__ENET2_TDATA00 | TX6UL_ENET_PAD_CTRL,
};

static const iomux_v3_cfg_t const tx6ul_i2c_pads[] = {
	/* internal I2C */
	MX6_PAD_SNVS_TAMPER1__GPIO5_IO01 | MUX_CFG_SION |
			TX6UL_I2C_PAD_CTRL, /* I2C SCL */
	MX6_PAD_SNVS_TAMPER0__GPIO5_IO00 | MUX_CFG_SION |
			TX6UL_I2C_PAD_CTRL, /* I2C SDA */
};

static const struct gpio const tx6ul_gpios[] = {
#ifdef CONFIG_SYS_I2C_SOFT
	/* These two entries are used to forcefully reinitialize the I2C bus */
	{ TX6UL_I2C1_SCL_GPIO, GPIOFLAG_INPUT, "I2C1 SCL", },
	{ TX6UL_I2C1_SDA_GPIO, GPIOFLAG_INPUT, "I2C1 SDA", },
#endif
	{ TX6UL_FEC_PWR_GPIO, GPIOFLAG_OUTPUT_INIT_HIGH, "FEC PHY PWR", },
	{ TX6UL_FEC_RST_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "FEC PHY RESET", },
	{ TX6UL_FEC_INT_GPIO, GPIOFLAG_INPUT, "FEC PHY INT", },
};

static const struct gpio const tx6ul_fec2_gpios[] = {
	{ TX6UL_FEC2_RST_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "FEC2 PHY RESET", },
	{ TX6UL_FEC2_INT_GPIO, GPIOFLAG_INPUT, "FEC2 PHY INT", },
};

#define GPIO_DR 0
#define GPIO_DIR 4
#define GPIO_PSR 8

/* run with default environment */
#if defined(TX6UL_I2C1_SCL_GPIO) && defined(TX6UL_I2C1_SDA_GPIO)
#define SCL_BANK	(TX6UL_I2C1_SCL_GPIO / 32)
#define SDA_BANK	(TX6UL_I2C1_SDA_GPIO / 32)
#define SCL_BIT		(1 << (TX6UL_I2C1_SCL_GPIO % 32))
#define SDA_BIT		(1 << (TX6UL_I2C1_SDA_GPIO % 32))

static void * const gpio_ports[] = {
	(void *)GPIO1_BASE_ADDR,
	(void *)GPIO2_BASE_ADDR,
	(void *)GPIO3_BASE_ADDR,
	(void *)GPIO4_BASE_ADDR,
	(void *)GPIO5_BASE_ADDR,
};

static void tx6ul_i2c_recover(void)
{
	int i;
	int bad = 0;
	struct gpio_regs *scl_regs = gpio_ports[SCL_BANK];
	struct gpio_regs *sda_regs = gpio_ports[SDA_BANK];

	if ((readl(&scl_regs->gpio_psr) & SCL_BIT) &&
	    (readl(&sda_regs->gpio_psr) & SDA_BIT))
		return;

	debug("Clearing I2C bus\n");
	if (!(readl(&scl_regs->gpio_psr) & SCL_BIT)) {
		printf("I2C SCL stuck LOW\n");
		bad++;

		setbits_le32(&scl_regs->gpio_dr, SCL_BIT);
		setbits_le32(&scl_regs->gpio_dir, SCL_BIT);

		imx_iomux_v3_setup_pad(MX6_PAD_SNVS_TAMPER1__GPIO5_IO01 |
				       MUX_CFG_SION | TX6UL_GPIO_OUT_PAD_CTRL);
	}
	if (!(readl(&sda_regs->gpio_psr) & SDA_BIT)) {
		printf("I2C SDA stuck LOW\n");
		bad++;

		clrbits_le32(&sda_regs->gpio_dir, SDA_BIT);
		setbits_le32(&scl_regs->gpio_dr, SCL_BIT);
		setbits_le32(&scl_regs->gpio_dir, SCL_BIT);

		udelay(5);

		for (i = 0; i < 18; i++) {
			u32 reg = readl(&scl_regs->gpio_dr) ^ SCL_BIT;

			debug("%sing SCL\n", (reg & SCL_BIT) ? "Sett" : "Clear");
			writel(reg, &scl_regs->gpio_dr);
			udelay(5);
			if (reg & SCL_BIT) {
				if (readl(&sda_regs->gpio_psr) & SDA_BIT)
					break;
				if (!(readl(&scl_regs->gpio_psr) & SCL_BIT))
					break;
				break;
			}
		}
	}
	if (bad) {
		bool scl = !!(readl(&scl_regs->gpio_psr) & SCL_BIT);
		bool sda = !!(readl(&sda_regs->gpio_psr) & SDA_BIT);

		if (scl && sda) {
			printf("I2C bus recovery succeeded\n");
		} else {
			printf("I2C bus recovery FAILED: SCL: %d SDA: %d\n",
			       scl, sda);
		}
		imx_iomux_v3_setup_multiple_pads(tx6ul_i2c_pads,
						 ARRAY_SIZE(tx6ul_i2c_pads));
	}
}
#else
static inline void tx6ul_i2c_recover(void)
{
}
#endif

/* placed in section '.data' to prevent overwriting relocation info
 * overlayed with bss
 */
static u32 wrsr __data;

#define WRSR_POR			(1 << 4)
#define WRSR_TOUT			(1 << 1)
#define WRSR_SFTW			(1 << 0)

static void print_reset_cause(void)
{
	struct src *src_regs = (struct src *)SRC_BASE_ADDR;
	void __iomem *wdt_base = (void __iomem *)WDOG1_BASE_ADDR;
	u32 srsr;
	char *dlm = "";

	printf("Reset cause: ");

	srsr = readl(&src_regs->srsr);
	wrsr = readw(wdt_base + 4);

	if (wrsr & WRSR_POR) {
		printf("%sPOR", dlm);
		dlm = " | ";
	}
	if (srsr & 0x00004) {
		printf("%sCSU", dlm);
		dlm = " | ";
	}
	if (srsr & 0x00008) {
		printf("%sIPP USER", dlm);
		dlm = " | ";
	}
	if (srsr & 0x00010) {
		if (wrsr & WRSR_SFTW) {
			printf("%sSOFT", dlm);
			dlm = " | ";
		}
		if (wrsr & WRSR_TOUT) {
			printf("%sWDOG", dlm);
			dlm = " | ";
		}
	}
	if (srsr & 0x00020) {
		printf("%sJTAG HIGH-Z", dlm);
		dlm = " | ";
	}
	if (srsr & 0x00040) {
		printf("%sJTAG SW", dlm);
		dlm = " | ";
	}
	if (srsr & 0x10000) {
		printf("%sWARM BOOT", dlm);
		dlm = " | ";
	}
	if (dlm[0] == '\0')
		printf("unknown");

	printf("\n");
}

#ifdef CONFIG_IMX6_THERMAL
#include <thermal.h>
#include <imx_thermal.h>
#include <fuse.h>

static void print_temperature(void)
{
	struct udevice *thermal_dev;
	int cpu_tmp, minc, maxc, ret;
	char const *grade_str;
	static u32 __data thermal_calib;

	puts("Temperature: ");
	switch (get_cpu_temp_grade(&minc, &maxc)) {
	case TEMP_AUTOMOTIVE:
		grade_str = "Automotive";
		break;
	case TEMP_INDUSTRIAL:
		grade_str = "Industrial";
		break;
	case TEMP_EXTCOMMERCIAL:
		grade_str = "Extended Commercial";
		break;
	default:
		grade_str = "Commercial";
	}
	printf("%s grade (%dC to %dC)", grade_str, minc, maxc);
	ret = uclass_get_device(UCLASS_THERMAL, 0, &thermal_dev);
	if (ret == 0) {
		ret = thermal_get_temp(thermal_dev, &cpu_tmp);

		if (ret == 0)
			printf(" at %dC", cpu_tmp);
		else
			puts(" - failed to read sensor data");
	} else {
		puts(" - no sensor device found");
	}

	if (fuse_read(1, 6, &thermal_calib) == 0) {
		printf(" - calibration data 0x%08x\n", thermal_calib);
	} else {
		puts(" - Failed to read thermal calib fuse\n");
	}
}
#else
static inline void print_temperature(void)
{
}
#endif

int checkboard(void)
{
	u32 cpurev = get_cpu_rev();
	char *cpu_str = "?";

	if (is_cpu_type(MXC_CPU_MX6SL))
		cpu_str = "SL";
	else if (is_cpu_type(MXC_CPU_MX6DL))
		cpu_str = "DL";
	else if (is_cpu_type(MXC_CPU_MX6SOLO))
		cpu_str = "SOLO";
	else if (is_cpu_type(MXC_CPU_MX6Q))
		cpu_str = "Q";
	else if (is_cpu_type(MXC_CPU_MX6UL))
		cpu_str = "UL";
	else if (is_cpu_type(MXC_CPU_MX6ULL))
		cpu_str = "ULL";

	printf("CPU:   Freescale i.MX6%s rev%d.%d at %d MHz\n",
		cpu_str,
		(cpurev & 0x000F0) >> 4,
		(cpurev & 0x0000F) >> 0,
		mxc_get_clock(MXC_ARM_CLK) / 1000000);

	print_temperature();
	print_reset_cause();
#ifdef CONFIG_MX6_TEMPERATURE_HOT
	check_cpu_temperature(1);
#endif
	tx6ul_i2c_recover();
	return 0;
}

/* serial port not initialized at this point */
int board_early_init_f(void)
{
	return 0;
}

#ifndef CONFIG_MX6_TEMPERATURE_HOT
static bool tx6ul_temp_check_enabled = true;
#else
#define tx6ul_temp_check_enabled	0
#endif

static inline u8 tx6ul_mem_suffix(void)
{
	return '0' + CONFIG_SYS_SDRAM_CHIP_SIZE / 1024 * 2 +
		IS_ENABLED(CONFIG_TX6_EMMC);
}

#ifdef CONFIG_RN5T567
/* PMIC settings */
#define VDD_RTC_VAL		rn5t_mV_to_regval_rtc(3000)
#define VDD_CORE_VAL		rn5t_mV_to_regval(1300)		/* DCDC1 */
#define VDD_CORE_VAL_LP		rn5t_mV_to_regval(900)
#define VDD_DDR_VAL		rn5t_mV_to_regval(1350)		/* DCDC3 SDRAM 1.35V */
#define VDD_DDR_VAL_LP		rn5t_mV_to_regval(1350)
#define VDD_IO_EXT_VAL		rn5t_mV_to_regval(3300)		/* DCDC4 eMMC/NAND,VDDIO_EXT 3.0V */
#define VDD_IO_EXT_VAL_LP	rn5t_mV_to_regval(3300)
#define VDD_IO_INT_VAL		rn5t_mV_to_regval2(3300)	/* LDO1 ENET,GPIO,LCD,SD1,UART,3.3V */
#define VDD_IO_INT_VAL_LP	rn5t_mV_to_regval2(3300)
#define VDD_ADC_VAL		rn5t_mV_to_regval2(3300)	/* LDO2 ADC */
#define VDD_ADC_VAL_LP		rn5t_mV_to_regval2(3300)
#define VDD_PMIC_VAL		rn5t_mV_to_regval2(2500)	/* LDO3 PMIC */
#define VDD_PMIC_VAL_LP		rn5t_mV_to_regval2(2500)
#define VDD_CSI_VAL		rn5t_mV_to_regval2(3300)	/* LDO4 CSI */
#define VDD_CSI_VAL_LP		rn5t_mV_to_regval2(3300)
#define VDD_LDO5_VAL		rn5t_mV_to_regval2(1200)	/* LDO5 1.2V */
#define LDOEN1_LDO1EN		(1 << 0)
#define LDOEN1_LDO2EN		(1 << 1)
#define LDOEN1_LDO3EN		(1 << 2)
#define LDOEN1_LDO4EN		(1 << 3)
#define LDOEN1_LDO5EN		(1 << 4)
#define LDOEN1_VAL		(LDOEN1_LDO1EN | LDOEN1_LDO2EN | LDOEN1_LDO3EN | LDOEN1_LDO4EN)
#define LDOEN1_MASK		0x1f
#define LDOEN2_LDORTC1EN	(1 << 4)
#define LDOEN2_LDORTC2EN	(1 << 5)
#define LDOEN2_VAL		LDOEN2_LDORTC1EN
#define LDOEN2_MASK		0x30

static struct pmic_regs rn5t567_regs[] = {
	{ RN5T567_NOETIMSET, NOETIMSET_DIS_OFF_NOE_TIM | 0x5, },
	{ RN5T567_SLPCNT, 0, },
	{ RN5T567_REPCNT, (3 << 4) | (0 << 1), },
	{ RN5T567_DC1DAC, VDD_CORE_VAL, },
	{ RN5T567_DC3DAC, VDD_DDR_VAL, },
	{ RN5T567_DC4DAC, VDD_IO_EXT_VAL, },
	{ RN5T567_DC1DAC_SLP, VDD_CORE_VAL_LP, },
	{ RN5T567_DC3DAC_SLP, VDD_DDR_VAL_LP, },
	{ RN5T567_DC4DAC_SLP, VDD_IO_EXT_VAL_LP, },
	{ RN5T567_DC1CTL, DCnCTL_EN | DCnCTL_DIS | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC2CTL, DCnCTL_DIS, },
	{ RN5T567_DC3CTL, DCnCTL_EN | DCnCTL_DIS | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC4CTL, DCnCTL_EN | DCnCTL_DIS | DCnMODE_SLP(MODE_PSM), },
	{ RN5T567_DC1CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_HIGH | DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW, },
	{ RN5T567_DC2CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_HIGH | DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW, },
	{ RN5T567_DC3CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_HIGH | DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW, },
	{ RN5T567_DC4CTL2, DCnCTL2_LIMSDEN | DCnCTL2_LIM_HIGH | DCnCTL2_SR_HIGH | DCnCTL2_OSC_LOW, },
	{ RN5T567_LDORTC1DAC, VDD_RTC_VAL, },
	{ RN5T567_LDORTC1_SLOT, 0x0f, ~0x3f, },
	{ RN5T567_LDO1DAC, VDD_IO_INT_VAL, },
	{ RN5T567_LDO2DAC, VDD_ADC_VAL, },
	{ RN5T567_LDO3DAC, VDD_PMIC_VAL, },
	{ RN5T567_LDO4DAC, VDD_CSI_VAL, },
	{ RN5T567_LDO1DAC_SLP, VDD_IO_INT_VAL_LP, },
	{ RN5T567_LDO2DAC_SLP, VDD_ADC_VAL_LP, },
	{ RN5T567_LDO3DAC_SLP, VDD_PMIC_VAL_LP, },
	{ RN5T567_LDO4DAC_SLP, VDD_CSI_VAL_LP, },
	{ RN5T567_LDO5DAC, VDD_LDO5_VAL, },
	{ RN5T567_LDO1DAC_SLP, VDD_IO_INT_VAL_LP, },
	{ RN5T567_LDO2DAC_SLP, VDD_ADC_VAL_LP, },
	{ RN5T567_LDO3DAC_SLP, VDD_PMIC_VAL_LP, },
	{ RN5T567_LDO4DAC_SLP, VDD_CSI_VAL_LP, },
	{ RN5T567_LDOEN1, LDOEN1_VAL, ~LDOEN1_MASK, },
	{ RN5T567_LDOEN2, LDOEN2_VAL, ~LDOEN2_MASK, },
	{ RN5T567_LDODIS, 0x1f, ~0x1f, },
	{ RN5T567_INTPOL, 0, },
	{ RN5T567_INTEN, 0x3, },
	{ RN5T567_DCIREN, 0xf, },
	{ RN5T567_EN_GPIR, 0, },
};

static int pmic_addr = 0x33;
#endif

int board_init(void)
{
	int ret;
	u32 cpurev = get_cpu_rev();
	char f = '?';

	if (is_cpu_type(MXC_CPU_MX6UL))
		f = ((cpurev & 0xff) > 0x10) ? '5' : '0';
	else if (is_cpu_type(MXC_CPU_MX6ULL))
		f = '8';

	debug("%s@%d: cpurev=%08x\n", __func__, __LINE__, cpurev);

	printf("Board: Ka-Ro TXUL-%c01%c\n", f, tx6ul_mem_suffix());

	get_hab_status();

	ret = gpio_request_array(tx6ul_gpios, ARRAY_SIZE(tx6ul_gpios));
	if (ret < 0)
		printf("Failed to request tx6ul_gpios: %d\n", ret);

	imx_iomux_v3_setup_multiple_pads(tx6ul_pads, ARRAY_SIZE(tx6ul_pads));

	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;
	gd->bd->bi_arch_number = -1;

	if (ctrlc() || (wrsr & WRSR_TOUT)) {
		if (wrsr & WRSR_TOUT)
			printf("WDOG RESET detected; Skipping PMIC setup\n");
		else
			printf("<CTRL-C> detected; safeboot enabled\n");
#ifndef CONFIG_MX6_TEMPERATURE_HOT
		tx6ul_temp_check_enabled = false;
#endif
		return 0;
	}

	ret = tx6_pmic_init(pmic_addr, rn5t567_regs, ARRAY_SIZE(rn5t567_regs));
	if (ret) {
		printf("Failed to setup PMIC voltages: %d\n", ret);
		hang();
	}
	return 0;
}

int dram_init(void)
{
	debug("%s@%d: \n", __func__, __LINE__);

	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				    PHYS_SDRAM_1_SIZE * CONFIG_NR_DRAM_BANKS);
	return 0;
}

void dram_init_banksize(void)
{
	debug("%s@%d: \n", __func__, __LINE__);

	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = get_ram_size((void *)PHYS_SDRAM_1,
					       PHYS_SDRAM_1_SIZE);
#if CONFIG_NR_DRAM_BANKS > 1
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = get_ram_size((void *)PHYS_SDRAM_2,
					       PHYS_SDRAM_2_SIZE);
#endif
}

#ifdef	CONFIG_FSL_ESDHC
#define TX6UL_SD_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_PUS_47K_UP |	\
					PAD_CTL_SPEED_MED |		\
					PAD_CTL_DSE_40ohm |		\
					PAD_CTL_SRE_FAST)

static const iomux_v3_cfg_t mmc0_pads[] = {
	MX6_PAD_SD1_CMD__USDHC1_CMD | TX6UL_SD_PAD_CTRL,
	MX6_PAD_SD1_CLK__USDHC1_CLK | TX6UL_SD_PAD_CTRL,
	MX6_PAD_SD1_DATA0__USDHC1_DATA0 | TX6UL_SD_PAD_CTRL,
	MX6_PAD_SD1_DATA1__USDHC1_DATA1 | TX6UL_SD_PAD_CTRL,
	MX6_PAD_SD1_DATA2__USDHC1_DATA2 | TX6UL_SD_PAD_CTRL,
	MX6_PAD_SD1_DATA3__USDHC1_DATA3 | TX6UL_SD_PAD_CTRL,
	/* SD1 CD */
	MX6_PAD_NAND_CE1_B__GPIO4_IO14 | TX6UL_SD_PAD_CTRL,
};

#ifdef CONFIG_TX6_EMMC
static const iomux_v3_cfg_t mmc1_pads[] = {
	MX6_PAD_NAND_WE_B__USDHC2_CMD | TX6UL_SD_PAD_CTRL,
	MX6_PAD_NAND_RE_B__USDHC2_CLK | TX6UL_SD_PAD_CTRL,
	MX6_PAD_NAND_DATA00__USDHC2_DATA0 | TX6UL_SD_PAD_CTRL,
	MX6_PAD_NAND_DATA01__USDHC2_DATA1 | TX6UL_SD_PAD_CTRL,
	MX6_PAD_NAND_DATA02__USDHC2_DATA2 | TX6UL_SD_PAD_CTRL,
	MX6_PAD_NAND_DATA03__USDHC2_DATA3 | TX6UL_SD_PAD_CTRL,
	/* eMMC RESET */
	MX6_PAD_NAND_ALE__USDHC2_RESET_B | MUX_PAD_CTRL(PAD_CTL_PUS_47K_UP |
							PAD_CTL_DSE_40ohm),
};
#endif

static struct tx6ul_esdhc_cfg {
	const iomux_v3_cfg_t *pads;
	int num_pads;
	enum mxc_clock clkid;
	struct fsl_esdhc_cfg cfg;
	int cd_gpio;
} tx6ul_esdhc_cfg[] = {
#ifdef CONFIG_TX6_EMMC
	{
		.pads = mmc1_pads,
		.num_pads = ARRAY_SIZE(mmc1_pads),
		.clkid = MXC_ESDHC2_CLK,
		.cfg = {
			.esdhc_base = (void __iomem *)USDHC2_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = -EINVAL,
	},
#endif
	{
		.pads = mmc0_pads,
		.num_pads = ARRAY_SIZE(mmc0_pads),
		.clkid = MXC_ESDHC_CLK,
		.cfg = {
			.esdhc_base = (void __iomem *)USDHC1_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = TX6UL_SD1_CD_GPIO,
	},
};

static inline struct tx6ul_esdhc_cfg *to_tx6ul_esdhc_cfg(struct fsl_esdhc_cfg *cfg)
{
	return container_of(cfg, struct tx6ul_esdhc_cfg, cfg);
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct tx6ul_esdhc_cfg *cfg = to_tx6ul_esdhc_cfg(mmc->priv);

	if (cfg->cd_gpio < 0)
		return 1;

	debug("SD card %d is %spresent (GPIO %d)\n",
	      cfg - tx6ul_esdhc_cfg,
	      gpio_get_value(cfg->cd_gpio) ? "NOT " : "",
	      cfg->cd_gpio);
	return !gpio_get_value(cfg->cd_gpio);
}

int board_mmc_init(bd_t *bis)
{
	int i;

	debug("%s@%d: \n", __func__, __LINE__);

#ifndef CONFIG_ENV_IS_IN_MMC
	if (!(gd->flags & GD_FLG_ENV_READY)) {
		printf("deferred ...");
		return 0;
	}
#endif
	for (i = 0; i < ARRAY_SIZE(tx6ul_esdhc_cfg); i++) {
		struct mmc *mmc;
		struct tx6ul_esdhc_cfg *cfg = &tx6ul_esdhc_cfg[i];
		int ret;

		cfg->cfg.sdhc_clk = mxc_get_clock(cfg->clkid);
		imx_iomux_v3_setup_multiple_pads(cfg->pads, cfg->num_pads);

		if (cfg->cd_gpio >= 0) {
			ret = gpio_request_one(cfg->cd_gpio,
					       GPIOFLAG_INPUT, "MMC CD");
			if (ret) {
				printf("Error %d requesting GPIO%d_%d\n",
				       ret, cfg->cd_gpio / 32,
				       cfg->cd_gpio % 32);
				continue;
			}
		}

		debug("%s: Initializing MMC slot %d\n", __func__, i);
		fsl_esdhc_initialize(bis, &cfg->cfg);

		mmc = find_mmc_device(i);
		if (mmc == NULL)
			continue;
		if (board_mmc_getcd(mmc))
			mmc_init(mmc);
	}
	return 0;
}
#endif /* CONFIG_FSL_ESDHC */

enum {
	LED_STATE_INIT = -1,
	LED_STATE_OFF,
	LED_STATE_ON,
	LED_STATE_ERR,
};

static inline int calc_blink_rate(void)
{
	if (!tx6ul_temp_check_enabled)
		return CONFIG_SYS_HZ;

	return CONFIG_SYS_HZ + CONFIG_SYS_HZ / 10 -
		(check_cpu_temperature(0) - TEMPERATURE_MIN) * CONFIG_SYS_HZ /
		(TEMPERATURE_HOT - TEMPERATURE_MIN);
}

void show_activity(int arg)
{
	static int led_state = LED_STATE_INIT;
	static int blink_rate;
	static ulong last;
	int ret;

	switch (led_state) {
	case LED_STATE_ERR:
		return;

	case LED_STATE_INIT:
		last = get_timer(0);
		ret = gpio_set_value(TX6UL_LED_GPIO, 1);
		if (ret)
			led_state = LED_STATE_ERR;
		else
			led_state = LED_STATE_ON;
		blink_rate = calc_blink_rate();
		break;

	case LED_STATE_ON:
	case LED_STATE_OFF:
		if (get_timer(last) > blink_rate) {
			blink_rate = calc_blink_rate();
			last = get_timer_masked();
			if (led_state == LED_STATE_ON) {
				gpio_set_value(TX6UL_LED_GPIO, 0);
			} else {
				gpio_set_value(TX6UL_LED_GPIO, 1);
			}
			led_state = 1 - led_state;
		}
		break;
	}
}

static const iomux_v3_cfg_t stk5_jtag_pads[] = {
	MX6_PAD_JTAG_MOD__SJC_MOD | TX6UL_GPIO_IN_PAD_CTRL,
	MX6_PAD_JTAG_TCK__SJC_TCK | TX6UL_GPIO_IN_PAD_CTRL,
	MX6_PAD_JTAG_TRST_B__SJC_TRSTB | TX6UL_GPIO_IN_PAD_CTRL,
	MX6_PAD_JTAG_TDI__SJC_TDI | TX6UL_GPIO_IN_PAD_CTRL,
	MX6_PAD_JTAG_TDO__SJC_TDO | TX6UL_GPIO_OUT_PAD_CTRL,
	MX6_PAD_JTAG_TMS__SJC_TMS | TX6UL_GPIO_IN_PAD_CTRL,
};

static const iomux_v3_cfg_t stk5_pads[] = {
	/* SW controlled LED on STK5 baseboard */
	MX6_PAD_SNVS_TAMPER9__GPIO5_IO09,

	/* I2C bus on DIMM pins 40/41 */
	MX6_PAD_GPIO1_IO01__I2C2_SDA | MUX_MODE_SION | TX6UL_I2C_PAD_CTRL,
	MX6_PAD_GPIO1_IO00__I2C2_SCL | MUX_MODE_SION | TX6UL_I2C_PAD_CTRL,

	/* TSC200x PEN IRQ */
	MX6_PAD_JTAG_TMS__GPIO1_IO11 | TX6UL_GPIO_IN_PAD_CTRL,

	/* EDT-FT5x06 Polytouch panel */
	MX6_PAD_SNVS_TAMPER2__GPIO5_IO02 | TX6UL_GPIO_IN_PAD_CTRL, /* IRQ */
	MX6_PAD_SNVS_TAMPER3__GPIO5_IO03 | TX6UL_GPIO_OUT_PAD_CTRL, /* RESET */
	MX6_PAD_SNVS_TAMPER8__GPIO5_IO08 | TX6UL_GPIO_OUT_PAD_CTRL, /* WAKE */

	/* USBH1 */
	MX6_PAD_GPIO1_IO02__USB_OTG2_PWR | TX6UL_GPIO_OUT_PAD_CTRL, /* VBUSEN */
	MX6_PAD_GPIO1_IO03__USB_OTG2_OC | TX6UL_GPIO_IN_PAD_CTRL, /* OC */

	/* USBOTG */
	MX6_PAD_UART3_CTS_B__GPIO1_IO26 | TX6UL_GPIO_OUT_PAD_CTRL, /* VBUSEN */
	MX6_PAD_UART3_RTS_B__GPIO1_IO27 | TX6UL_GPIO_IN_PAD_CTRL, /* OC */
};

static const struct gpio stk5_gpios[] = {
	{ TX6UL_LED_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "HEARTBEAT LED", },

	{ IMX_GPIO_NR(1, 27), GPIOFLAG_INPUT, "USBOTG OC", },
	{ IMX_GPIO_NR(1, 26), GPIOFLAG_OUTPUT_INIT_LOW, "USBOTG VBUS enable", },
};

static const iomux_v3_cfg_t tx_tester_pads[] = {
	/* SW controlled LEDs on TX-TESTER-V5 baseboard */
	MX6_PAD_SNVS_TAMPER4__GPIO5_IO04, /* red LED */
	MX6_PAD_SNVS_TAMPER9__GPIO5_IO09, /* yellow LED */
	MX6_PAD_SNVS_TAMPER8__GPIO5_IO08, /* green LED */

	MX6_PAD_LCD_DATA04__GPIO3_IO09, /* IO_RESET */

	/* I2C bus on DIMM pins 40/41 */
	MX6_PAD_GPIO1_IO01__I2C2_SDA | MUX_MODE_SION | TX6UL_I2C_PAD_CTRL,
	MX6_PAD_GPIO1_IO00__I2C2_SCL | MUX_MODE_SION | TX6UL_I2C_PAD_CTRL,

	/* USBH1 */
	MX6_PAD_GPIO1_IO02__USB_OTG2_PWR | TX6UL_GPIO_OUT_PAD_CTRL, /* VBUSEN */
	MX6_PAD_GPIO1_IO03__USB_OTG2_OC | TX6UL_GPIO_IN_PAD_CTRL, /* OC */

	/* USBOTG */
	MX6_PAD_UART3_CTS_B__GPIO1_IO26 | TX6UL_GPIO_OUT_PAD_CTRL, /* VBUSEN */
	MX6_PAD_UART3_RTS_B__GPIO1_IO27 | TX6UL_GPIO_IN_PAD_CTRL, /* OC */

	MX6_PAD_LCD_DATA08__GPIO3_IO13 | TX6UL_GPIO_OUT_PAD_CTRL,
	MX6_PAD_LCD_DATA09__GPIO3_IO14 | TX6UL_GPIO_OUT_PAD_CTRL,
	MX6_PAD_LCD_DATA10__GPIO3_IO15 | TX6UL_GPIO_OUT_PAD_CTRL,

	/* USBH_VBUSEN */
	MX6_PAD_LCD_DATA11__GPIO3_IO16 | TX6UL_GPIO_OUT_PAD_CTRL,

	/*
	 * no drive capability for DUT_ETN_LINKLED, DUT_ETN_ACTLED
	 * to not interfere whith the DUT ETN PHY strap pins
	 */
	MX6_PAD_SNVS_TAMPER2__GPIO5_IO02, MUX_PAD_CTRL(PAD_CTL_HYS |
						       PAD_CTL_DSE_DISABLE |
						       PAD_CTL_SPEED_LOW),
	MX6_PAD_SNVS_TAMPER3__GPIO5_IO03, MUX_PAD_CTRL(PAD_CTL_HYS |
						       PAD_CTL_DSE_DISABLE |
						       PAD_CTL_SPEED_LOW),
};

static const struct gpio tx_tester_gpios[] = {
	{ TX6UL_LED_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "LEDGE#", },
	{ IMX_GPIO_NR(5, 4), GPIOFLAG_OUTPUT_INIT_LOW, "LEDRT#", },
	{ IMX_GPIO_NR(5, 8), GPIOFLAG_OUTPUT_INIT_LOW, "LEDGN#", },

	{ IMX_GPIO_NR(1, 26), GPIOFLAG_OUTPUT_INIT_HIGH, "PMIC PWR_ON", },

	{ IMX_GPIO_NR(3, 5), GPIOFLAG_INPUT, "TSTART#", },
	{ IMX_GPIO_NR(3, 6), GPIOFLAG_INPUT, "STARTED", },
	{ IMX_GPIO_NR(3, 7), GPIOFLAG_INPUT, "TSTOP#", },
	{ IMX_GPIO_NR(3, 8), GPIOFLAG_OUTPUT_INIT_LOW, "STOP#", },

	{ IMX_GPIO_NR(3, 10), GPIOFLAG_INPUT, "DUT_PGOOD", },

	{ IMX_GPIO_NR(3, 11), GPIOFLAG_OUTPUT_INIT_HIGH, "VBACKUP_OFF", },
	{ IMX_GPIO_NR(3, 12), GPIOFLAG_OUTPUT_INIT_LOW, "VBACKUP_LOAD", },

	{ IMX_GPIO_NR(1, 10), GPIOFLAG_OUTPUT_INIT_LOW, "VOUTLOAD1", },
	{ IMX_GPIO_NR(3, 30), GPIOFLAG_OUTPUT_INIT_LOW, "VOUTLOAD2", },
	{ IMX_GPIO_NR(3, 31), GPIOFLAG_OUTPUT_INIT_LOW, "VOUTLOAD3", },

	{ IMX_GPIO_NR(3, 13), GPIOFLAG_OUTPUT_INIT_LOW, "VIOLOAD1", },
	{ IMX_GPIO_NR(3, 14), GPIOFLAG_OUTPUT_INIT_LOW, "VIOLOAD2", },
	{ IMX_GPIO_NR(3, 15), GPIOFLAG_OUTPUT_INIT_LOW, "VIOLOAD3", },
};

#ifdef CONFIG_LCD
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 4096,
	.vl_row = 1024,

	.vl_bpix = LCD_COLOR32,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
};

static struct fb_videomode tx6ul_fb_modes[] = {
#ifndef CONFIG_SYS_LVDS_IF
	{
		/* Standard VGA timing */
		.name		= "VGA",
		.refresh	= 60,
		.xres		= 640,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(25175),
		.left_margin	= 48,
		.hsync_len	= 96,
		.right_margin	= 16,
		.upper_margin	= 31,
		.vsync_len	= 2,
		.lower_margin	= 12,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ETV570 640 x 480 display. Syncs low active,
		 * DE high active, 115.2 mm x 86.4 mm display area
		 * VGA compatible timing
		 */
		.name		= "ETV570",
		.refresh	= 60,
		.xres		= 640,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(25175),
		.left_margin	= 114,
		.hsync_len	= 30,
		.right_margin	= 16,
		.upper_margin	= 32,
		.vsync_len	= 3,
		.lower_margin	= 10,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0350G0DH6 320 x 240 display.
		 * 70.08 mm x 52.56 mm display area.
		 */
		.name		= "ET0350",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= KHZ2PICOS(6500),
		.left_margin	= 68 - 34,
		.hsync_len	= 34,
		.right_margin	= 20,
		.upper_margin	= 18 - 3,
		.vsync_len	= 3,
		.lower_margin	= 4,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0430G0DH6 480 x 272 display.
		 * 95.04 mm x 53.856 mm display area.
		 */
		.name		= "ET0430",
		.refresh	= 60,
		.xres		= 480,
		.yres		= 272,
		.pixclock	= KHZ2PICOS(9000),
		.left_margin	= 2,
		.hsync_len	= 41,
		.right_margin	= 2,
		.upper_margin	= 2,
		.vsync_len	= 10,
		.lower_margin	= 2,
	},
	{
		/* Emerging ET0500G0DH6 800 x 480 display.
		 * 109.6 mm x 66.4 mm display area.
		 */
		.name		= "ET0500",
		.refresh	= 60,
		.xres		= 800,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),
		.left_margin	= 216 - 128,
		.hsync_len	= 128,
		.right_margin	= 1056 - 800 - 216,
		.upper_margin	= 35 - 2,
		.vsync_len	= 2,
		.lower_margin	= 525 - 480 - 35,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ETQ570G0DH6 320 x 240 display.
		 * 115.2 mm x 86.4 mm display area.
		 */
		.name		= "ETQ570",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= KHZ2PICOS(6400),
		.left_margin	= 38,
		.hsync_len	= 30,
		.right_margin	= 30,
		.upper_margin	= 16, /* 15 according to datasheet */
		.vsync_len	= 3, /* TVP -> 1>x>5 */
		.lower_margin	= 4, /* 4.5 according to datasheet */
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0700G0DH6 800 x 480 display.
		 * 152.4 mm x 91.44 mm display area.
		 */
		.name		= "ET0700",
		.refresh	= 60,
		.xres		= 800,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),
		.left_margin	= 216 - 128,
		.hsync_len	= 128,
		.right_margin	= 1056 - 800 - 216,
		.upper_margin	= 35 - 2,
		.vsync_len	= 2,
		.lower_margin	= 525 - 480 - 35,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET070001DM6 800 x 480 display.
		 * 152.4 mm x 91.44 mm display area.
		 */
		.name		= "ET070001DM6",
		.refresh	= 60,
		.xres		= 800,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),
		.left_margin	= 216 - 128,
		.hsync_len	= 128,
		.right_margin	= 1056 - 800 - 216,
		.upper_margin	= 35 - 2,
		.vsync_len	= 2,
		.lower_margin	= 525 - 480 - 35,
		.sync		= 0,
	},
#else
	{
		/* HannStar HSD100PXN1
		 * 202.7m mm x 152.06 mm display area.
		 */
		.name		= "HSD100PXN1",
		.refresh	= 60,
		.xres		= 1024,
		.yres		= 768,
		.pixclock	= KHZ2PICOS(65000),
		.left_margin	= 0,
		.hsync_len	= 0,
		.right_margin	= 320,
		.upper_margin	= 0,
		.vsync_len	= 0,
		.lower_margin	= 38,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
#endif
	{
		/* unnamed entry for assigning parameters parsed from 'video_mode' string */
		.refresh	= 60,
		.left_margin	= 48,
		.hsync_len	= 96,
		.right_margin	= 16,
		.upper_margin	= 31,
		.vsync_len	= 2,
		.lower_margin	= 12,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
};

static int lcd_enabled = 1;
static int lcd_bl_polarity;

static int lcd_backlight_polarity(void)
{
	return lcd_bl_polarity;
}

static const iomux_v3_cfg_t stk5_lcd_pads[] = {
#ifdef CONFIG_LCD
	/* LCD RESET */
	MX6_PAD_LCD_RESET__GPIO3_IO04 | TX6UL_GPIO_OUT_PAD_CTRL,
	/* LCD POWER_ENABLE */
	MX6_PAD_SNVS_TAMPER4__GPIO5_IO04 | TX6UL_GPIO_OUT_PAD_CTRL,
	/* LCD Backlight (PWM) */
	MX6_PAD_NAND_DQS__GPIO4_IO16 | TX6UL_GPIO_OUT_PAD_CTRL,
	/* Display */
	MX6_PAD_LCD_DATA00__LCDIF_DATA00,
	MX6_PAD_LCD_DATA01__LCDIF_DATA01,
	MX6_PAD_LCD_DATA02__LCDIF_DATA02,
	MX6_PAD_LCD_DATA03__LCDIF_DATA03,
	MX6_PAD_LCD_DATA04__LCDIF_DATA04,
	MX6_PAD_LCD_DATA05__LCDIF_DATA05,
	MX6_PAD_LCD_DATA06__LCDIF_DATA06,
	MX6_PAD_LCD_DATA07__LCDIF_DATA07,
	MX6_PAD_LCD_DATA08__LCDIF_DATA08,
	MX6_PAD_LCD_DATA09__LCDIF_DATA09,
	MX6_PAD_LCD_DATA10__LCDIF_DATA10,
	MX6_PAD_LCD_DATA11__LCDIF_DATA11,
	MX6_PAD_LCD_DATA12__LCDIF_DATA12,
	MX6_PAD_LCD_DATA13__LCDIF_DATA13,
	MX6_PAD_LCD_DATA14__LCDIF_DATA14,
	MX6_PAD_LCD_DATA15__LCDIF_DATA15,
	MX6_PAD_LCD_DATA16__LCDIF_DATA16,
	MX6_PAD_LCD_DATA17__LCDIF_DATA17,
	MX6_PAD_LCD_DATA18__LCDIF_DATA18,
	MX6_PAD_LCD_DATA19__LCDIF_DATA19,
	MX6_PAD_LCD_DATA20__LCDIF_DATA20,
	MX6_PAD_LCD_DATA21__LCDIF_DATA21,
	MX6_PAD_LCD_DATA22__LCDIF_DATA22,
	MX6_PAD_LCD_DATA23__LCDIF_DATA23,
	MX6_PAD_LCD_HSYNC__LCDIF_HSYNC, /* HSYNC */
	MX6_PAD_LCD_VSYNC__LCDIF_VSYNC, /* VSYNC */
	MX6_PAD_LCD_ENABLE__LCDIF_ENABLE, /* OE_ACD */
	MX6_PAD_LCD_CLK__LCDIF_CLK, /* LSCLK */
#endif
};

static const struct gpio stk5_lcd_gpios[] = {
	{ TX6UL_LCD_RST_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "LCD RESET", },
	{ TX6UL_LCD_PWR_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "LCD POWER", },
	{ TX6UL_LCD_BACKLIGHT_GPIO, GPIOFLAG_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

/* run with valid env from NAND/eMMC */
void lcd_enable(void)
{
	/* HACK ALERT:
	 * global variable from common/lcd.c
	 * Set to 0 here to prevent messages from going to LCD
	 * rather than serial console
	 */
	lcd_is_enabled = 0;

	if (lcd_enabled) {
		karo_load_splashimage(1);

		debug("Switching LCD on\n");
		gpio_set_value(TX6UL_LCD_PWR_GPIO, 1);
		udelay(100);
		gpio_set_value(TX6UL_LCD_RST_GPIO, 1);
		udelay(300000);
		gpio_set_value(TX6UL_LCD_BACKLIGHT_GPIO,
			       lcd_backlight_polarity());
	}
}

static void lcd_disable(void)
{
	if (lcd_enabled) {
		printf("Disabling LCD\n");
		panel_info.vl_row = 0;
		lcd_enabled = 0;
	}
}

void lcd_ctrl_init(void *lcdbase)
{
	int color_depth = 24;
	const char *video_mode = karo_get_vmode(getenv("video_mode"));
	const char *vm;
	unsigned long val;
	int refresh = 60;
	struct fb_videomode *p = &tx6ul_fb_modes[0];
	struct fb_videomode fb_mode;
	int xres_set = 0, yres_set = 0, bpp_set = 0, refresh_set = 0;

	if (!lcd_enabled) {
		debug("LCD disabled\n");
		return;
	}

	if (had_ctrlc() || (wrsr & WRSR_TOUT)) {
		lcd_disable();
		setenv("splashimage", NULL);
		return;
	}

	karo_fdt_move_fdt();
	lcd_bl_polarity = karo_fdt_get_backlight_polarity(working_fdt);

	if (video_mode == NULL) {
		lcd_disable();
		return;
	}
	vm = video_mode;
	if (karo_fdt_get_fb_mode(working_fdt, video_mode, &fb_mode) == 0) {
		p = &fb_mode;
		debug("Using video mode from FDT\n");
		vm += strlen(vm);
		if (fb_mode.xres > panel_info.vl_col ||
			fb_mode.yres > panel_info.vl_row) {
			printf("video resolution from DT: %dx%d exceeds hardware limits: %dx%d\n",
			       fb_mode.xres, fb_mode.yres,
			       panel_info.vl_col, panel_info.vl_row);
			lcd_enabled = 0;
			return;
		}
	}
	if (p->name != NULL)
		debug("Trying compiled-in video modes\n");
	while (p->name != NULL) {
		if (strcmp(p->name, vm) == 0) {
			debug("Using video mode: '%s'\n", p->name);
			vm += strlen(vm);
			break;
		}
		p++;
	}
	if (*vm != '\0')
		debug("Trying to decode video_mode: '%s'\n", vm);
	while (*vm != '\0') {
		if (*vm >= '0' && *vm <= '9') {
			char *end;

			val = simple_strtoul(vm, &end, 0);
			if (end > vm) {
				if (!xres_set) {
					if (val > panel_info.vl_col)
						val = panel_info.vl_col;
					p->xres = val;
					panel_info.vl_col = val;
					xres_set = 1;
				} else if (!yres_set) {
					if (val > panel_info.vl_row)
						val = panel_info.vl_row;
					p->yres = val;
					panel_info.vl_row = val;
					yres_set = 1;
				} else if (!bpp_set) {
					switch (val) {
					case 8:
					case 16:
					case 18:
					case 24:
					case 32:
						color_depth = val;
						break;

					default:
						printf("Invalid color depth: '%.*s' in video_mode; using default: '%u'\n",
						       end - vm, vm,
						       color_depth);
					}
					bpp_set = 1;
				} else if (!refresh_set) {
					refresh = val;
					refresh_set = 1;
				}
			}
			vm = end;
		}
		switch (*vm) {
		case '@':
			bpp_set = 1;
			/* fallthru */
		case '-':
			yres_set = 1;
			/* fallthru */
		case 'x':
			xres_set = 1;
			/* fallthru */
		case 'M':
		case 'R':
			vm++;
			break;

		default:
			if (*vm != '\0')
				vm++;
		}
	}
	if (p->xres == 0 || p->yres == 0) {
		printf("Invalid video mode: %s\n", getenv("video_mode"));
		lcd_enabled = 0;
		printf("Supported video modes are:");
		for (p = &tx6ul_fb_modes[0]; p->name != NULL; p++) {
			printf(" %s", p->name);
		}
		printf("\n");
		return;
	}
	if (p->xres > panel_info.vl_col || p->yres > panel_info.vl_row) {
		printf("video resolution: %dx%d exceeds hardware limits: %dx%d\n",
		       p->xres, p->yres, panel_info.vl_col, panel_info.vl_row);
		lcd_enabled = 0;
		return;
	}
	panel_info.vl_col = p->xres;
	panel_info.vl_row = p->yres;

	switch (color_depth) {
	case 8:
		panel_info.vl_bpix = LCD_COLOR8;
		break;
	case 16:
		panel_info.vl_bpix = LCD_COLOR16;
		break;
	default:
		panel_info.vl_bpix = LCD_COLOR32;
	}

	if (refresh_set || p->pixclock == 0)
		p->pixclock = KHZ2PICOS(refresh *
					(p->xres + p->left_margin +
					 p->right_margin + p->hsync_len) *
					(p->yres + p->upper_margin +
					 p->lower_margin + p->vsync_len) /
					1000);
	debug("Pixel clock set to %lu.%03lu MHz\n",
	      PICOS2KHZ(p->pixclock) / 1000, PICOS2KHZ(p->pixclock) % 1000);

	if (p != &fb_mode) {
		int ret;

		debug("Creating new display-timing node from '%s'\n",
		      video_mode);
		ret = karo_fdt_create_fb_mode(working_fdt, video_mode, p);
		if (ret)
			printf("Failed to create new display-timing node from '%s': %d\n",
			       video_mode, ret);
	}

	gpio_request_array(stk5_lcd_gpios, ARRAY_SIZE(stk5_lcd_gpios));
	imx_iomux_v3_setup_multiple_pads(stk5_lcd_pads,
					 ARRAY_SIZE(stk5_lcd_pads));

	debug("video format: %ux%u-%u@%u\n", p->xres, p->yres,
	      color_depth, refresh);

	if (karo_load_splashimage(0) == 0) {
		char vmode[128];

		/* setup env variable for mxsfb display driver */
		snprintf(vmode, sizeof(vmode),
			 "x:%d,y:%d,le:%d,ri:%d,up:%d,lo:%d,hs:%d,vs:%d,sync:%d,pclk:%d,depth:%d",
			 p->xres, p->yres, p->left_margin, p->right_margin,
			 p->upper_margin, p->lower_margin, p->hsync_len,
			 p->vsync_len, p->sync, p->pixclock, color_depth);
		setenv("videomode", vmode);

		debug("Initializing LCD controller\n");
		lcdif_clk_enable();
		video_hw_init();
		setenv("videomode", NULL);
	} else {
		debug("Skipping initialization of LCD controller\n");
	}
}
#else
#define lcd_enabled 0
#endif /* CONFIG_LCD */

#ifndef CONFIG_ENV_IS_IN_MMC
static void tx6ul_mmc_init(void)
{
	puts("MMC:   ");
	if (board_mmc_init(gd->bd) < 0)
		cpu_mmc_init(gd->bd);
	print_mmc_devices(',');
}
#else
static inline void tx6ul_mmc_init(void)
{
}
#endif

static void stk5_board_init(void)
{
	int ret;

	ret = gpio_request_array(stk5_gpios, ARRAY_SIZE(stk5_gpios));
	if (ret < 0) {
		printf("Failed to request stk5_gpios: %d\n", ret);
		return;
	}

	imx_iomux_v3_setup_multiple_pads(stk5_pads, ARRAY_SIZE(stk5_pads));
	if (getenv_yesno("jtag_enable") != 0) {
		/* true if unset or set to one of: 'yYtT1' */
		imx_iomux_v3_setup_multiple_pads(stk5_jtag_pads, ARRAY_SIZE(stk5_jtag_pads));
	}

	debug("%s@%d: \n", __func__, __LINE__);
}

static void stk5v3_board_init(void)
{
	debug("%s@%d: \n", __func__, __LINE__);
	stk5_board_init();
	debug("%s@%d: \n", __func__, __LINE__);
	tx6ul_mmc_init();
}

static void stk5v5_board_init(void)
{
	int ret;

	stk5_board_init();
	tx6ul_mmc_init();

	ret = gpio_request_one(IMX_GPIO_NR(3, 5), GPIOFLAG_OUTPUT_INIT_HIGH,
			       "Flexcan Transceiver");
	if (ret) {
		printf("Failed to request Flexcan Transceiver GPIO: %d\n", ret);
		return;
	}

	imx_iomux_v3_setup_pad(MX6_PAD_LCD_DATA00__GPIO3_IO05 |
			       TX6UL_GPIO_OUT_PAD_CTRL);
}

static void tx_tester_board_init(void)
{
	int ret;

	setenv("video_mode", NULL);
	setenv("touchpanel", NULL);
	if (getenv("eth1addr") && !getenv("ethprime"))
		setenv("ethprime", "FEC1");

	ret = gpio_request_array(tx_tester_gpios, ARRAY_SIZE(tx_tester_gpios));
	if (ret) {
		printf("Failed to request TX-Tester GPIOs: %d\n", ret);
		return;
	}
	imx_iomux_v3_setup_multiple_pads(stk5_pads, ARRAY_SIZE(stk5_pads));

	if (wrsr & WRSR_TOUT)
		gpio_set_value(IMX_GPIO_NR(5, 4), 1);

	if (getenv_yesno("jtag_enable") != 0) {
		/* true if unset or set to one of: 'yYtT1' */
		imx_iomux_v3_setup_multiple_pads(stk5_jtag_pads,
						 ARRAY_SIZE(stk5_jtag_pads));
	}

	gpio_set_value(IMX_GPIO_NR(3, 8), 1);
}

static void tx6ul_set_cpu_clock(void)
{
	unsigned long cpu_clk = getenv_ulong("cpu_clk", 10, 0);

	if (cpu_clk == 0 || cpu_clk == mxc_get_clock(MXC_ARM_CLK) / 1000000)
		return;

	if (had_ctrlc() || (wrsr & WRSR_TOUT)) {
		printf("%s detected; skipping cpu clock change\n",
		       (wrsr & WRSR_TOUT) ? "WDOG RESET" : "<CTRL-C>");
		return;
	}
	if (mxc_set_clock(CONFIG_SYS_MX6_HCLK, cpu_clk, MXC_ARM_CLK) == 0) {
		cpu_clk = mxc_get_clock(MXC_ARM_CLK);
		printf("CPU clock set to %lu.%03lu MHz\n",
		       cpu_clk / 1000000, cpu_clk / 1000 % 1000);
	} else {
		printf("Error: Failed to set CPU clock to %lu MHz\n", cpu_clk);
	}
}

int board_late_init(void)
{
	const char *baseboard;

	debug("%s@%d: \n", __func__, __LINE__);

	env_cleanup();

	if (tx6ul_temp_check_enabled)
		check_cpu_temperature(1);

	tx6ul_set_cpu_clock();

	if (had_ctrlc())
		setenv_ulong("safeboot", 1);
	else if (wrsr & WRSR_TOUT)
		setenv_ulong("wdreset", 1);
	else
		karo_fdt_move_fdt();

	baseboard = getenv("baseboard");
	if (!baseboard)
		goto exit;

	printf("Baseboard: %s\n", baseboard);

	if (strncmp(baseboard, "stk5", 4) == 0) {
		if ((strlen(baseboard) == 4) ||
			strcmp(baseboard, "stk5-v3") == 0) {
			stk5v3_board_init();
		} else if (strcmp(baseboard, "stk5-v5") == 0) {
			const char *otg_mode = getenv("otg_mode");

			if (otg_mode && strcmp(otg_mode, "host") == 0) {
				printf("otg_mode='%s' is incompatible with baseboard %s; setting to 'none'\n",
				       otg_mode, baseboard);
				setenv("otg_mode", "none");
			}
			stk5v5_board_init();
		} else {
			printf("WARNING: Unsupported STK5 board rev.: %s\n",
				baseboard + 4);
		}
	} else if (strncmp(baseboard, "ulmb-", 5) == 0) {
			const char *otg_mode = getenv("otg_mode");

			if (otg_mode && strcmp(otg_mode, "host") == 0) {
				printf("otg_mode='%s' is incompatible with baseboard %s; setting to 'none'\n",
				       otg_mode, baseboard);
				setenv("otg_mode", "none");
			}
			stk5_board_init();
	} else if (strncmp(baseboard, "tx-tester-", 10) == 0) {
			const char *otg_mode = getenv("otg_mode");

			if (!otg_mode || strcmp(otg_mode, "none") != 0)
				setenv("otg_mode", "device");
			tx_tester_board_init();
	} else {
		printf("WARNING: Unsupported baseboard: '%s'\n",
			baseboard);
		printf("Reboot with <CTRL-C> pressed to fix this\n");
		if (!had_ctrlc())
			return -EINVAL;
	}

exit:
	debug("%s@%d: \n", __func__, __LINE__);

	clear_ctrlc();
	return 0;
}

#ifdef CONFIG_FEC_MXC

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	unsigned int mac0, mac1, mac2;
	unsigned int __maybe_unused fuse3_override, fuse4_override;

	memset(mac, 0, 6);

	switch (dev_id) {
	case 0:
		if (fuse_read(4, 2, &mac0)) {
			printf("Failed to read MAC0 fuse\n");
			return;
		}
		if (fuse_read(4, 3, &mac1)) {
			printf("Failed to read MAC1 fuse\n");
			return;
		}
		mac[0] = mac1 >> 8;
		mac[1] = mac1;
		mac[2] = mac0 >> 24;
		mac[3] = mac0 >> 16;
		mac[4] = mac0 >> 8;
		mac[5] = mac0;
		break;

	case 1:
		if (fuse_read(4, 3, &mac1)) {
			printf("Failed to read MAC1 fuse\n");
			return;
		}
		debug("read %08x from fuse 3\n", mac1);
		if (fuse_read(4, 4, &mac2)) {
			printf("Failed to read MAC2 fuse\n");
			return;
		}
		debug("read %08x from fuse 4\n", mac2);
		mac[0] = mac2 >> 24;
		mac[1] = mac2 >> 16;
		mac[2] = mac2 >> 8;
		mac[3] = mac2;
		mac[4] = mac1 >> 24;
		mac[5] = mac1 >> 16;
		break;

	default:
		return;
	}
	debug("%s@%d: Done %d %pM\n", __func__, __LINE__, dev_id, mac);
}

static void tx6ul_init_mac(void)
{
	u8 mac[ETH_ALEN];
	const char *baseboard = getenv("baseboard");

	imx_get_mac_from_fuse(0, mac);
	if (!is_valid_ethaddr(mac)) {
		printf("No valid MAC address programmed\n");
		return;
	}
	printf("MAC addr from fuse: %pM\n", mac);
	if (!getenv("ethaddr"))
		eth_setenv_enetaddr("ethaddr", mac);

	if (!baseboard || strncmp(baseboard, "stk5", 4) == 0) {
		setenv("eth1addr", NULL);
		return;
	}
	if (getenv("eth1addr"))
		return;
	imx_get_mac_from_fuse(1, mac);
	if (is_valid_ethaddr(mac))
		eth_setenv_enetaddr("eth1addr", mac);
}

int board_eth_init(bd_t *bis)
{
	int ret;

	tx6ul_init_mac();

	/* delay at least 21ms for the PHY internal POR signal to deassert */
	udelay(22000);

	imx_iomux_v3_setup_multiple_pads(tx6ul_enet1_pads,
					 ARRAY_SIZE(tx6ul_enet1_pads));

	/* Deassert RESET to the external phys */
	gpio_set_value(TX6UL_FEC_RST_GPIO, 1);

	if (getenv("ethaddr")) {
		ret = fecmxc_initialize_multi(bis, 0, 0, ENET_BASE_ADDR);
		if (ret) {
			printf("failed to initialize FEC0: %d\n", ret);
			return ret;
		}
	}
	if (getenv("eth1addr")) {
		ret = gpio_request_array(tx6ul_fec2_gpios,
					 ARRAY_SIZE(tx6ul_fec2_gpios));
		if (ret < 0) {
			printf("Failed to request tx6ul_fec2_gpios: %d\n", ret);
		}
		imx_iomux_v3_setup_multiple_pads(tx6ul_enet2_pads,
						 ARRAY_SIZE(tx6ul_enet2_pads));

		writel(0x00100000, 0x020c80e4); /* assert ENET2_125M_EN */

		/* Minimum PHY reset duration */
		udelay(100);
		gpio_set_value(TX6UL_FEC2_RST_GPIO, 1);
		/* Wait for PHY internal POR to finish */
		udelay(22000);

		ret = fecmxc_initialize_multi(bis, 1, 2, ENET2_BASE_ADDR);
		if (ret) {
			printf("failed to initialize FEC1: %d\n", ret);
			return ret;
		}
	}
	return 0;
}
#endif /* CONFIG_FEC_MXC */

#ifdef CONFIG_SERIAL_TAG
void get_board_serial(struct tag_serialnr *serialnr)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank0_regs *fuse = (void *)ocotp->bank[0].fuse_regs;

	serialnr->low = readl(&fuse->cfg0);
	serialnr->high = readl(&fuse->cfg1);
}
#endif

#if defined(CONFIG_OF_BOARD_SETUP)
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
#include <jffs2/jffs2.h>
#include <mtd_node.h>
static struct node_info nodes[] = {
	{ "fsl,imx6q-gpmi-nand", MTD_DEV_TYPE_NAND, },
};
#else
#define fdt_fixup_mtdparts(b,n,c) do { } while (0)
#endif

static const char *tx6ul_touchpanels[] = {
	"ti,tsc2007",
	"edt,edt-ft5x06",
	"eeti,egalax_ts",
};

int ft_board_setup(void *blob, bd_t *bd)
{
	const char *baseboard = getenv("baseboard");
	int stk5_v5 = baseboard != NULL && (strcmp(baseboard, "stk5-v5") == 0);
	const char *video_mode = karo_get_vmode(getenv("video_mode"));
	int ret;

	ret = fdt_increase_size(blob, 4096);
	if (ret) {
		printf("Failed to increase FDT size: %s\n", fdt_strerror(ret));
		return ret;
	}
	if (stk5_v5)
		karo_fdt_enable_node(blob, "stk5led", 0);

	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));

	karo_fdt_fixup_touchpanel(blob, tx6ul_touchpanels,
				  ARRAY_SIZE(tx6ul_touchpanels));
	karo_fdt_fixup_usb_otg(blob, "usbotg", "fsl,usbphy", "vbus-supply");
	karo_fdt_fixup_flexcan(blob, stk5_v5);

	karo_fdt_update_fb_mode(blob, video_mode, "/lcd-panel");

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */
