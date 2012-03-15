/*
 * Copyright (C) 2011 Lothar Waßmann <LW@KARO-electronics.de>
 * based on: board/freesclae/mx28_evk.c (C) 2010 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/iomux-mx28.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/regs-pinctrl.h>
#include <asm/arch/regs-clkctrl.h>
#include <asm/arch/regs-ocotp.h>
#include <asm/arch/sys_proto.h>

#include <mmc.h>
#include <netdev.h>
#include <imx_ssp_mmc.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Functions
 */
int board_early_init_f(void)
{
	/* IO0 clock at 480MHz */
	mx28_set_ioclk(MXC_IOCLK0, 480000);
	/* IO1 clock at 480MHz */
	mx28_set_ioclk(MXC_IOCLK1, 480000);

	/* SSP0 clock at 96MHz */
	mx28_set_sspclk(MXC_SSPCLK0, 96000, 0);
	/* SSP2 clock at 96MHz */
	mx28_set_sspclk(MXC_SSPCLK2, 96000, 0);

	return 0;
}

void coloured_LED_init(void)
{
	/* Switch LED off */
	gpio_set_value(MX28_PAD_ENET0_RXD3__GPIO_4_10, 0);
}

int board_init(void)
{
	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;
	return 0;
}

int dram_init(void)
{
	return mx28_dram_init();
}

#ifdef	CONFIG_CMD_MMC
static int tx28_mmc_wp(int dev_no)
{
	return 0;
}

int board_mmc_init(bd_t *bis)
{
	return mxsmmc_initialize(bis, 0, tx28_mmc_wp);
}
#endif /* CONFIG_CMD_MMC */

#ifdef CONFIG_FEC_MXC
#ifdef CONFIG_GET_FEC_MAC_ADDR_FROM_IIM

#ifdef CONFIG_FEC_MXC_MULTI
#define FEC_MAX_IDX			1
#else
#define FEC_MAX_IDX			0
#endif

static int fec_get_mac_addr(int index)
{
	u32 val1, val2;
	int timeout = 1000;
	struct mx28_ocotp_regs *ocotp_regs =
		(struct mx28_ocotp_regs *)MXS_OCOTP_BASE;
	u32 *cust = &ocotp_regs->hw_ocotp_cust0;
	char mac[6 * 3];
	char env_name[] = "eth.addr";

	if (index < 0 || index > FEC_MAX_IDX)
		return -EINVAL;

	/* set this bit to open the OTP banks for reading */
	writel(OCOTP_CTRL_RD_BANK_OPEN,
		&ocotp_regs->hw_ocotp_ctrl_set);

	/* wait until OTP contents are readable */
	while (OCOTP_CTRL_BUSY & readl(&ocotp_regs->hw_ocotp_ctrl)) {
		if (timeout-- < 0)
			return -ETIMEDOUT;
		udelay(100);
	}

	val1 = readl(&cust[index * 8]);
	val2 = readl(&cust[index * 8 + 4]);
	if ((val1 | val2) == 0)
		return 0;
	snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
		(val1 >> 24) & 0xFF, (val1 >> 16) & 0xFF,
		(val1 >> 8) & 0xFF, (val1 >> 0) & 0xFF,
		(val2 >> 24) & 0xFF, (val2 >> 16) & 0xFF);
	if (index == 0)
		snprintf(env_name, sizeof(env_name), "ethaddr");
	else
		snprintf(env_name, sizeof(env_name), "eth%daddr", index);

	setenv(env_name, mac);
	return 0;
}
#endif /* CONFIG_GET_FEC_MAC_ADDR_FROM_IIM */

static iomux_cfg_t tx28_fec_pads[] = {
	MX28_PAD_ENET0_RX_EN__ENET0_RX_EN,
	MX28_PAD_ENET0_RXD0__ENET0_RXD0,
	MX28_PAD_ENET0_RXD1__ENET0_RXD1,
};

int board_eth_init(bd_t *bis)
{
	int ret;

	/* Reset the external phy */
	gpio_direction_output(MX28_PAD_ENET0_RX_CLK__GPIO_4_13, 0);

	/* Power on the external phy */
	gpio_direction_output(MX28_PAD_PWM4__GPIO_3_29, 1);

	/* Pull strap pins to high */
	gpio_direction_output(MX28_PAD_ENET0_RX_EN__GPIO_4_2, 1);
	gpio_direction_output(MX28_PAD_ENET0_RXD0__GPIO_4_3, 1);
	gpio_direction_output(MX28_PAD_ENET0_RXD1__GPIO_4_4, 1);
	gpio_direction_input(MX28_PAD_ENET0_TX_CLK__GPIO_4_5);

	udelay(25000);
	gpio_set_value(MX28_PAD_ENET0_RX_CLK__GPIO_4_13, 1);
	udelay(100);

	mxs_iomux_setup_multiple_pads(tx28_fec_pads, ARRAY_SIZE(tx28_fec_pads));

	ret = cpu_eth_init(bis);
	if (ret) {
		printf("cpu_eth_init() failed: %d\n", ret);
		return ret;
	}

	ret = fec_get_mac_addr(0);
	if (ret < 0) {
		printf("Failed to read FEC0 MAC address from OCOTP\n");
		return ret;
	}
#ifdef CONFIG_FEC_MXC_MULTI
	if (getenv("ethaddr")) {
		ret = fecmxc_initialize_multi(bis, 0, 0, MXS_ENET0_BASE);
		if (ret) {
			printf("FEC MXS: Unable to init FEC0\n");
			return ret;
		}
	}

	ret = fec_get_mac_addr(1);
	if (ret < 0) {
		printf("Failed to read FEC1 MAC address from OCOTP\n");
		return ret;
	}
	if (getenv("eth1addr")) {
		ret = fecmxc_initialize_multi(bis, 1, 1, MXS_ENET1_BASE);
		if (ret) {
			printf("FEC MXS: Unable to init FEC1\n");
			return ret;
		}
	}
	return 0;
#else
	if (getenv("ethaddr")) {
		ret = fecmxc_initialize(bis);
	}
	return ret;
#endif
}
#endif /* CONFIG_FEC_MXC */

enum {
	LED_STATE_INIT = -1,
	LED_STATE_OFF,
	LED_STATE_ON,
};

void show_activity(int arg)
{
	static int led_state = LED_STATE_INIT;
	static ulong last;

	if (led_state == LED_STATE_INIT) {
		last = get_timer(0);
		gpio_set_value(MX28_PAD_ENET0_RXD3__GPIO_4_10, 1);
		led_state = LED_STATE_ON;
	} else {
		if (get_timer(last) > CONFIG_SYS_HZ) {
			last = get_timer(0);
			if (led_state == LED_STATE_ON) {
				gpio_set_value(MX28_PAD_ENET0_RXD3__GPIO_4_10, 0);
			} else {
				gpio_set_value(MX28_PAD_ENET0_RXD3__GPIO_4_10, 1);
			}
			led_state = 1 - led_state;
		}
	}
}
