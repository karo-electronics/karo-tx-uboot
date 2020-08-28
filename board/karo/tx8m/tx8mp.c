// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <console.h>
#include <dwc3-uboot.h>
#include <errno.h>
#include <fdt_support.h>
#include <fsl_esdhc_imx.h>
#include <fsl_wdog.h>
#include <fuse.h>
#include <i2c.h>
#include <led.h>
#include <malloc.h>
#include <miiphy.h>
#include <mipi_dsi.h>
#include <mmc.h>
#include <netdev.h>
#include <spl.h>
#include <thermal.h>
#include <usb.h>
#include <asm-generic/gpio.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx8mp_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/armv8/mmu.h>
#include <asm/mach-imx/dma.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/mach-imx/video.h>
#include <asm/setup.h>
#include <asm/bootm.h>
#include <dm/device_compat.h>
#include <dm/uclass.h>
#include <power/regulator.h>
#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

enum tx8m_boardtype {
	QSXP,
	QSBASE3,
	NUM_BOARD_TYPES
};

#if IS_ENABLED(CONFIG_NET)
static int qsxp_setup_eth(enum tx8m_boardtype board)
{
	struct iomuxc_gpr_base_regs *iomuxc_gpr_regs =
		(void *)IOMUXC_GPR_BASE_ADDR;
	unsigned char mac[6];

	if (board == QSXP) {
		/* Use 50M anatop REF_CLK1 for ENET, not from external */
		setbits_le32(&iomuxc_gpr_regs->gpr[1],
			     IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_MASK);
	} else {
		/* Use 125M anatop REF_CLK1 for ENET, not from external */
		setbits_le32(&iomuxc_gpr_regs->gpr[1],
			     IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_MASK);
	}

	if (!env_get("ethaddr")) {
		imx_get_mac_from_fuse(0, mac);
		printf("MAC addr: %pM\n", mac);
	} else {
		printf("MAC addr: %s\n", env_get("ethaddr"));
	}
	return 0;
}
#else
static inline int qsxp_setup_eth(enum tx8m_boardtype board)
{
	return 0;
}
#endif /* CONFIG_NET */

#if IS_ENABLED(CONFIG_DWC_ETH_QOS)
static int setup_eqos(void)
{
	struct iomuxc_gpr_base_regs *gpr =
		(struct iomuxc_gpr_base_regs *)IOMUXC_GPR_BASE_ADDR;

	/* set INTF as RGMII, enable RGMII TXC clock */
	clrsetbits_le32(&gpr->gpr[1],
			IOMUXC_GPR_GPR1_GPR_ENET_QOS_INTF_SEL_MASK, BIT(16));
	setbits_le32(&gpr->gpr[1], BIT(19) | BIT(21));

	return set_clk_eqos(ENET_125MHZ);
}
#else
static inline int setup_eqos(void)
{
	return 0;
}
#endif

int board_fix_fdt(void *blob)
{
	return 0;
}

static struct udevice *thermaldev __section(.data);

#define TEMPERATURE_HOT		80
#define TEMPERATURE_MIN		-40

static void qsxp_show_temp(void)
{
	int ret;
	int cpu_temp;
	static int last_temp = INT_MAX;
	static int avg_count = -1;

	ret = uclass_get_device_by_name(UCLASS_THERMAL, "cpu-thermal",
					&thermaldev);
	if (ret) {
		printf("Failed to find THERMAL device: %d\n", ret);
		return;
	}
	if (!thermaldev || thermal_get_temp(thermaldev, &cpu_temp))
		return;

	if (avg_count < 0)
		avg_count = 0;
	printf("CPU temperature: %d C\n", cpu_temp);
	if (last_temp == INT_MAX) {
		last_temp = cpu_temp;
	} else if (cpu_temp != last_temp) {
		static int cpu_temps[4] = { -1, };

		if (thermal_get_temp(thermaldev, &cpu_temps[avg_count]))
			return;
		if (++avg_count >= ARRAY_SIZE(cpu_temps)) {
			int bad = -1;
			int i;

			for (i = 0; i < avg_count; i++) {
				if (cpu_temp != cpu_temps[i])
					bad = i;
			}
			if (bad < 0) {
				debug("CPU temperature changed from %d to %d\n",
				      last_temp, cpu_temp);
				last_temp = cpu_temp;
			} else {
				debug("Spurious CPU temperature reading %d -> %d -> %d\n",
				      cpu_temp, cpu_temps[bad],
				      cpu_temps[i - 1]);
			}
			avg_count = 0;
		}
	} else {
		avg_count = 0;
	}
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("Board: Ka-Ro QSXP-ML81\n");

	ctrlc();
	if (gd->flags & GD_FLG_RELOC)
		printf("post-reloc\n");
	else
		printf("PRE-RELOC!\n");
	qsxp_show_temp();
	return 0;
}
#endif

#ifdef CONFIG_USB_DWC3
#define USB_PHY_CTRL0			0xF0040
#define USB_PHY_CTRL0_REF_SSP_EN	BIT(2)

#define USB_PHY_CTRL1			0xF0044
#define USB_PHY_CTRL1_RESET		BIT(0)
#define USB_PHY_CTRL1_COMMONONN		BIT(1)
#define USB_PHY_CTRL1_ATERESET		BIT(3)
#define USB_PHY_CTRL1_VDATSRCENB0	BIT(19)
#define USB_PHY_CTRL1_VDATDETENB0	BIT(20)

#define USB_PHY_CTRL2			0xF0048
#define USB_PHY_CTRL2_TXENABLEN0	BIT(8)

#define USB_PHY_CTRL6			0xF0058

#define HSIO_GPR_BASE				    0x32F10000U
#define HSIO_GPR_REG_0				    HSIO_GPR_BASE
#define HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN_SHIFT    1
#define HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN	    (0x1U << HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN_SHIFT)

static struct dwc3_device dwc3_device_data = {
	.maximum_speed = USB_SPEED_SUPER,
	.base = USB1_BASE_ADDR,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.index = 0,
	.power_down_scale = 2,
};

int usb_gadget_handle_interrupts(void)
{
	dwc3_uboot_handle_interrupt(0);
	return 0;
}

static void dwc3_nxp_usb_phy_init(struct dwc3_device *dwc3)
{
	u32 RegData;

	/* enable usb clock via hsio gpr */
	RegData = readl(HSIO_GPR_REG_0);
	RegData |= HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN;
	writel(RegData, HSIO_GPR_REG_0);

	/* USB3.0 PHY signal fsel for 100M ref */
	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData = (RegData & 0xfffff81f) | (0x2a << 5);
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL6);
	RegData &= ~0x1;
	writel(RegData, dwc3->base + USB_PHY_CTRL6);

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_VDATSRCENB0 | USB_PHY_CTRL1_VDATDETENB0 |
			USB_PHY_CTRL1_COMMONONN);
	RegData |= USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET;
	writel(RegData, dwc3->base + USB_PHY_CTRL1);

	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData |= USB_PHY_CTRL0_REF_SSP_EN;
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL2);
	RegData |= USB_PHY_CTRL2_TXENABLEN0;
	writel(RegData, dwc3->base + USB_PHY_CTRL2);

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET);
	writel(RegData, dwc3->base + USB_PHY_CTRL1);
}
#endif

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)

static struct udevice *reg_vbus;
static struct udevice *usbdev;

static int board_enable_vbus(int index, bool enable)
{
	int ret;

	if (reg_vbus)
		return 0;

	debug("Looking for USBH VBUS regulator\n");
	ret = uclass_get_device(UCLASS_USB, index, &usbdev);
	if (ret) {
		printf("Failed to get USB device: %d\n", ret);
		return ret;
	}
	ret = uclass_get_device_by_phandle(UCLASS_REGULATOR,
					   usbdev,
					   "vbus-supply",
					   &reg_vbus);
	if (ret) {
		dev_err(usbdev, "Failed to find VBUS regulator: %d\n",
			ret);
		return ret;
	}

	dev_dbg(reg_vbus, "%sabling USBH VBUS regulator\n",
		enable ? "En" : "Dis");
	ret = regulator_set_enable(reg_vbus, enable);
	if (ret)
		dev_err(reg_vbus, "Failed to %sable VBUS regulator: %d\n",
			enable ? "en" : "dis", ret);
	return ret;
}

int board_usb_init(int index, enum usb_init_type init)
{
	imx8m_usb_power(index, true);

	if (index == 0 && init == USB_INIT_DEVICE) {
		dwc3_nxp_usb_phy_init(&dwc3_device_data);
		return dwc3_uboot_init(&dwc3_device_data);
	} else if (index == 0 && init == USB_INIT_HOST) {
		return 0;
	} else if (index == 1 && init == USB_INIT_HOST) {
		board_enable_vbus(index, true);
	}

	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	if (index == 0 && init == USB_INIT_DEVICE)
		dwc3_uboot_exit(index);
	else if (index == 1 && init == USB_INIT_HOST)
		board_enable_vbus(index, false);

	imx8m_usb_power(index, false);

	return 0;
}
#endif /* CONFIG_USB_DWC3 */

#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				13
#define MIPI				15

int board_init(void)
{
#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)
	init_usb_clk();
#endif

	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true, 0);
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true, 0);

	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_R
int board_early_init_r(void)
{
	if (ctrlc())
		printf("<CTRL-C> detected; safeboot enabled\n");
	return 0;
}
#endif

int board_late_init(void)
{
	int ret;
	struct src *src_regs = (void *)SRC_BASE_ADDR;
	struct watchdog_regs *wdog = (void *)WDOG1_BASE_ADDR;
	u32 srsr = readl(&src_regs->srsr);
	u16 wrsr = readw(&wdog->wrsr);
	const char *fdt_file = env_get("fdt_file");
	enum tx8m_boardtype board = QSXP;
	const char *baseboard = env_get("baseboard");

	if (baseboard && strcmp(baseboard, "qsbase3") == 0)
		board = QSBASE3;

	karo_env_cleanup();
	if (srsr & 0x10 && !(wrsr & WRSR_SFTW)) {
		printf("Watchog reset detected; reboot required!\n");
		env_set("wdreset", "1");
	}
	if (had_ctrlc()) {
		env_set("safeboot", "1");
		fdt_file = NULL;
	}
	if (fdt_file) {
		ret = karo_load_fdt(fdt_file);
		if (ret)
			printf("Failed to load FDT from '%s': %d\n",
			       fdt_file, ret);
	}

	setup_eqos();
	qsxp_setup_eth(board);

	clear_ctrlc();
	return 0;
}

#ifdef CONFIG_ENV_IS_IN_MMC
int board_mmc_get_env_dev(int devno)
{
	return CONFIG_SYS_MMC_ENV_DEV;
}
#endif

/* provide mmc device number for fastboot */
int mmc_map_to_kernel_blk(int devno)
{
	return devno;
}

#ifdef CONFIG_OF_BOARD_SETUP

int ft_board_setup(void *blob, bd_t *bd)
{
	struct tag_serialnr serno;
	char serno_str[64 / 4 + 1];

	get_board_serial(&serno);
	snprintf(serno_str, sizeof(serno_str), "%08x%08x",
		 serno.high, serno.low);

	printf("serial-number: %s\n", serno_str);

	fdt_setprop(blob, 0, "serial-number", serno_str, strlen(serno_str));
	fsl_fdt_fixup_dr_usb(blob, bd);
	return 0;
}
#endif /* OF_BOARD_SETUP */
