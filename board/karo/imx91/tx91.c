// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2024 Marcel Mertens <MM@KARO-electronics.de>
 */

#include <env.h>
#include <init.h>
#include <malloc.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/global_data.h>
#include <asm/arch-imx9/ccm_regs.h>
#include <asm/arch-imx9/imx91_pins.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/uclass.h>
#include <linux/delay.h>
#include <power/pmic.h>

#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

int board_phys_sdram_size(phys_size_t *size)
{
	*size = PHYS_SDRAM_SIZE;
	return 0;
}

int board_interface_eth_init(struct udevice *dev, phy_interface_t interface)
{
	int ret;
	int mode;
	struct blk_ctrl_wakeupmix_regs *bctrl =
		(struct blk_ctrl_wakeupmix_regs *)BLK_CTRL_WAKEUPMIX_BASE_ADDR;
	int index;

	dev_dbg(dev, "%s@%d: interface='%s'\n", __func__, __LINE__,
		phy_string_for_interface(interface));

	ret = dev_read_alias_seq(dev, &index);
	if (ret < 0) {
		printf("Failed to get alias id for '%s': %d\n", dev->name, ret);
		return ret;
	}

	karo_set_ethaddr(index);

	if (!device_is_compatible(dev, "nxp,imx93-dwmac-eqos"))
		return 0;

	switch (interface) {
	case PHY_INTERFACE_MODE_RMII:
		/* set INTF as RMII */
		mode = BCTRL_GPR_ENET_QOS_INTF_SEL_RMII;
		set_clk_eqos(ENET_25MHZ);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		/* set INTF as RGMII, enable RGMII TXC clock */
		mode = BCTRL_GPR_ENET_QOS_INTF_SEL_RGMII;
		break;
	default:
		dev_err(dev, "Invalid phy interface mode: '%s'\n",
			phy_string_for_interface(interface));
		return -EINVAL;
	}
	clrsetbits_le32(&bctrl->eqos_gpr,
			BCTRL_GPR_ENET_QOS_INTF_MODE_MASK,
			mode | BCTRL_GPR_ENET_QOS_CLK_GEN_EN);
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_UART_BOARD_INIT)
#define UART_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_DSE(6) | PAD_CTL_FSEL2)
#define WDOG_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_DSE(6) | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static const iomux_v3_cfg_t uart_pads[] = {
	MX91_PAD_UART1_RXD__LPUART1_RX | UART_PAD_CTRL,
	MX91_PAD_UART1_TXD__LPUART1_TX | UART_PAD_CTRL,
};

void board_debug_uart_init(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
	init_uart_clk(LPUART1_CLK_ROOT);
}
#endif

#define SRC_BASE_ADDR		0x44460000
#define SRC_GPR1		0x54
#define SRSR_POR_B		BIT(0)
#define SRSR_IPP_USER_RESET_B	BIT(2)
#define SRSR_WDOG1_RST_B	BIT(3)
#define SRSR_WDOG2_RST_B	BIT(4)
#define SRSR_WDOG3_RST_B	BIT(5)
#define SRSR_WDOG4_RST_B	BIT(6)
#define SRSR_WDOG5_RST_B	BIT(7)
#define SRSR_TEMPSENSE_RST_B	BIT(8)
#define SRSR_CSU_RST_B		BIT(9)
#define SRSR_JTAG_RST_B		BIT(10)
#define SRSR_JTAG_SW_RST_B	BIT(12)
#define SRSR_RESET_MASK		0x000017fd

int board_init(void)
{
	debug("%s@%d\n", __func__, __LINE__);
	return 0;
}

int board_late_init(void)
{
	int ret;
	const char *fdt_file;

	karo_env_cleanup();

	ctrlc();
	if (had_ctrlc()) {
		env_set("safeboot", "1");
		printf("<CTRL-C> detected; safeboot enabled\n");
		clear_ctrlc();
		return 0;
	}

	karo_set_ethaddr(0);

	if (IS_ENABLED(CONFIG_KARO_UBOOT_MFG))
		goto out;

	fdt_file = env_get("fdt_file");
	ret = karo_load_fdt(fdt_file ?: CONFIG_DEFAULT_FDT_FILE);
	if (ret)
		printf("Failed to load fdt file: '%s': %d\n",
		       fdt_file ?: CONFIG_DEFAULT_FDT_FILE, ret);
out:
	return 0;
}
