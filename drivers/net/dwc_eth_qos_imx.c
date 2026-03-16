// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2022 NXP
 */

#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <eth_phy.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <reset.h>
#include <wait_bit.h>
#include <asm/arch/clock.h>
#include <asm/cache.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/mach-imx/sys_proto.h>
#include <linux/delay.h>

#include "dwc_eth_qos.h"

static struct clk_ref eqos_imx_clks[] = {
	{ "stmmaceth", offsetof(struct eqos_priv, clk_master_bus), },
	{ "ptp_ref", offsetof(struct eqos_priv, clk_ptp_ref), },
	{ "tx", offsetof(struct eqos_priv, clk_tx), },
	{ "pclk", offsetof(struct eqos_priv, clk_ck), },
};

static ulong eqos_get_tick_clk_rate_imx(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	return clk_get_rate(&eqos->clk_master_bus);
}

static int eqos_get_clks(struct udevice *dev)
{
	int ret;
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct clk_ref *clkref = eqos->clkrefs;
	size_t num_clks = eqos->num_clks;
	ssize_t i;

	for (i = 0; i < num_clks; i++) {
		const char *name = clkref[i].name;
		struct clk *clk = ((void *)eqos) + clkref[i].offset;

		ret = clk_get_by_name(dev, name, clk);
		if (ret) {
			pr_err("clk_get_by_name(%s) failed: %d", name, ret);
			return ret;
		}
		dev_dbg(dev, "Got '%s' clk\n", name);
	}
	return 0;
}

static int eqos_probe_resources_imx(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	phy_interface_t interface;
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = eqos_get_base_addr_dt(dev);
	if (ret) {
		dev_dbg(dev, "eqos_get_base_addr_dt failed: %d", ret);
		goto err_probe;
	}

	interface = eqos->config->interface(dev);

	if (interface == PHY_INTERFACE_MODE_NA) {
		pr_err("Invalid PHY interface\n");
		return -EINVAL;
	}

	eqos->clkrefs = eqos_imx_clks;
	eqos->num_clks = ARRAY_SIZE(eqos_imx_clks);

	ret = eqos_get_clks(dev);
	if (ret)
		return ret;

	ret = board_interface_eth_init(dev, interface);
	if (ret)
		return ret;

	eqos->max_speed = dev_read_u32_default(dev, "max-speed", 0);

	eqos->phy_reset_gpio = devm_gpiod_get_optional(dev, "phy-reset",
						       GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (IS_ERR(eqos->phy_reset_gpio)) {
		ret = PTR_ERR(eqos->phy_reset_gpio);
		dev_err(dev, "failed to request 'phy-reset' gpio: %d\n", ret);
		goto err_probe;
	}

	debug("%s: OK\n", __func__);
	return 0;

err_probe:
	dev_dbg(dev, "%s() failed: %d\n", __func__, ret);
	return ret;
}

static int eqos_remove_resources_imx(struct udevice *dev)
{
	debug("%s(dev=%p):\n", __func__, dev);
	return 0;
}

static int _eqos_stop_clks_imx(struct udevice *dev, size_t index)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct clk_ref *clkref = eqos->clkrefs;
	int ret;
	int i;

	dev_dbg(dev, "%s:\n", __func__);

	for (i = index - 1; i >= 0; i--) {
		struct clk *clk = ((void *)eqos) + clkref[i].offset;
		const char *name = clkref[i].name;

		ret = clk_disable(clk);
		if (ret < 0)
			pr_err("clk_disable(%s) failed: %d\n", name, ret);
	}

	debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_stop_clks_imx(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	return _eqos_stop_clks_imx(dev, eqos->num_clks);
}

static int eqos_start_clks_imx(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct clk_ref *clkref = eqos->clkrefs;
	int ret;
	size_t num_clks = eqos->num_clks;
	int i;

	dev_dbg(dev, "%s:\n", __func__);

	for (i = 0; i < num_clks; i++) {
		struct clk *clk = ((void *)eqos) + clkref[i].offset;
		const char *name = clkref[i].name;

		ret = clk_enable(clk);
		if (ret < 0) {
			dev_dbg(dev, "clk_enable(%s) failed: %d\n", name, ret);
			_eqos_stop_clks_imx(dev, i);
			return ret;
		}
	}
	dev_dbg(dev, "%s: OK\n", __func__);
	return 0;
}

static int eqos_start_resets_imx(struct udevice *dev)
{
	int ret;
	struct eqos_priv *eqos = dev_get_priv(dev);
	u32 reset_duration = dev_read_u32_default(dev, "phy-reset-duration", 100);
	u32 reset_post_delay = dev_read_u32_default(dev, "phy-reset-post-delay", 0);

	if (!eqos->phy_reset_gpio)
		return 0;

	ret = dm_gpio_set_value(eqos->phy_reset_gpio, 1);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, assert) failed: %d\n", ret);
		return ret;
	}

	udelay(reset_duration);

	ret = dm_gpio_set_value(eqos->phy_reset_gpio, 0);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, deassert) failed: %d\n", ret);
		return ret;
	}
	udelay(reset_post_delay);
	return 0;
}

static int eqos_stop_resets_imx(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	dm_gpio_set_value(eqos->phy_reset_gpio, 1);
	return 0;
}

static int eqos_set_tx_clk_speed_imx(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	ulong rate;
	int ret;

	if (device_is_compatible(dev, "nxp,imx93-dwmac-eqos"))
		return 0;

	debug("%s(dev=%p):\n", __func__, dev);

	if (eqos->phy->interface == PHY_INTERFACE_MODE_RMII)
		rate = 5000;	/* 5000 kHz = 5 MHz */
	else
		rate = 2500;	/* 2500 kHz = 2.5 MHz */

	if (eqos->phy->speed == SPEED_1000 &&
	    (eqos->phy->interface == PHY_INTERFACE_MODE_RGMII ||
	     eqos->phy->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	     eqos->phy->interface == PHY_INTERFACE_MODE_RGMII_RXID ||
	     eqos->phy->interface == PHY_INTERFACE_MODE_RGMII_TXID)) {
		rate *= 50;	/* Use 50x base rate i.e. 125 MHz */
	} else if (eqos->phy->speed == SPEED_100) {
		rate *= 10;	/* Use 10x base rate */
	} else if (eqos->phy->speed == SPEED_10) {
		rate *= 1;	/* Use base rate */
	} else {
		pr_err("invalid speed %d\n", eqos->phy->speed);
		return -EINVAL;
	}

	rate *= 1000;	/* clk_set_rate() operates in Hz */

	ret = clk_set_rate(&eqos->clk_tx, rate);
	if (ret < 0) {
		pr_err("imx (tx_clk, %lu) failed: %d\n", rate, ret);
		return ret;
	}

	return 0;
}

static int eqos_get_enetaddr_imx(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);

	imx_get_mac_from_fuse(dev_seq(dev), pdata->enetaddr);

	return 0;
}

static void eqos_fix_soc_reset_imx(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	if (IS_ENABLED(CONFIG_IMX93)) {
		/*
		 * Workaround for ERR051683 in i.MX93
		 * The i.MX93 requires speed configuration bits to be set to
		 * complete the reset procedure in RMII mode.
		 * See b536f32b5b03 ("net: stmmac: dwmac-imx: use platform
		 * specific reset for imx93 SoCs") in linux
		 */
		if (eqos->config->interface(dev) == PHY_INTERFACE_MODE_RMII) {
			udelay(200);
			setbits_le32(&eqos->mac_regs->configuration,
				     EQOS_MAC_CONFIGURATION_PS |
				     EQOS_MAC_CONFIGURATION_FES);
		}
	}
}

static struct eqos_ops eqos_imx_ops = {
	.eqos_inval_desc = eqos_inval_desc_generic,
	.eqos_flush_desc = eqos_flush_desc_generic,
	.eqos_inval_buffer = eqos_inval_buffer_generic,
	.eqos_flush_buffer = eqos_flush_buffer_generic,
	.eqos_probe_resources = eqos_probe_resources_imx,
	.eqos_remove_resources = eqos_remove_resources_imx,
	.eqos_stop_resets = eqos_stop_resets_imx,
	.eqos_start_resets = eqos_start_resets_imx,
	.eqos_stop_clks = eqos_stop_clks_imx,
	.eqos_start_clks = eqos_start_clks_imx,
	.eqos_calibrate_pads = eqos_null_ops,
	.eqos_disable_calibration = eqos_null_ops,
	.eqos_set_tx_clk_speed = eqos_set_tx_clk_speed_imx,
	.eqos_get_enetaddr = eqos_get_enetaddr_imx,
	.eqos_get_tick_clk_rate = eqos_get_tick_clk_rate_imx,
	.eqos_fix_soc_reset = eqos_fix_soc_reset_imx,
};

struct eqos_config __maybe_unused eqos_imx_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 10,
	.swr_wait = 50,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_250_300,
	.axi_bus_width = EQOS_AXI_WIDTH_64,
	.interface = dev_read_phy_mode,
	.ops = &eqos_imx_ops,
};
