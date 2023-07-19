// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2023 Lothar Waßmann <LW@KARO-electronics.de>
 */

#include <common.h>
#include <dwc3-uboot.h>
#include <env.h>
#include <env_internal.h>
#include <fb_fsl.h>
#include <init.h>
#include <miiphy.h>
#include <netdev.h>
#include <usb.h>
#include <asm/global_data.h>
#include <asm/arch-imx9/ccm_regs.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch-imx9/imx93_pins.h>
#include <asm/arch/clock.h>
#include <power/pmic.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/uclass.h>

#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL	(PAD_CTL_DSE(6) | PAD_CTL_FSEL2)
#define WDOG_PAD_CTRL	(PAD_CTL_DSE(6) | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static const iomux_v3_cfg_t uart_pads[] = {
	MX93_PAD_UART1_RXD__LPUART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX93_PAD_UART1_TXD__LPUART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

int board_early_init_f(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

	init_uart_clk(LPUART1_CLK_ROOT);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

static void karo_set_ethaddr(int index)
{
	unsigned char env_mac[ETH_ALEN];
	unsigned char fuse_mac[ETH_ALEN];

	imx_get_mac_from_fuse(index, fuse_mac);
	if (eth_env_get_enetaddr_by_index("eth", index, env_mac)) {
		printf("MAC addr: %pM\n", env_mac);
		if (is_valid_ethaddr(fuse_mac) && memcmp(env_mac, fuse_mac, sizeof(env_mac)))
			printf("MAC from env differs from fuse settings: %pM\n", fuse_mac);
	} else {
		if (is_valid_ethaddr(fuse_mac)) {
			printf("Using MAC addr from fuse\n");
			printf("MAC addr: %pM\n", fuse_mac);
			eth_env_set_enetaddr_by_index("eth", index, fuse_mac);
		} else {
			printf("No MAC addr found for eth%d\n", index);
		}
	}
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

	if (!device_is_compatible(dev, "fsl,imx-eqos"))
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
	if (IS_ENABLED(CONFIG_KARO_UBOOT_MFG))
		return 0;

	fdt_file = env_get("fdt_file");
	ret = karo_load_fdt(fdt_file ?: CONFIG_DEFAULT_FDT_FILE);
	if (ret)
		printf("Failed to load fdt file: '%s': %d\n",
		       fdt_file ?: CONFIG_DEFAULT_FDT_FILE, ret);
	return 0;
}
