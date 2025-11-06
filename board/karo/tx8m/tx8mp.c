// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <console.h>
#include <debug_uart.h>
#include <dwc3-uboot.h>
#include <errno.h>
#include <fdt_support.h>
#include <fsl_esdhc_imx.h>
#include <fsl_wdog.h>
#include <fuse.h>
#include <dwc3-uboot.h>
#include <i2c.h>
#include <led.h>
#include <malloc.h>
#include <miiphy.h>
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
#include <dm/device_compat.h>
#include <dm/uclass.h>
#include <linux/arm-smccc.h>
#include <power/regulator.h>
#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

#if IS_ENABLED(CONFIG_NET)
int board_interface_eth_init(struct udevice *dev, phy_interface_t interface_type)
{
	int ret;
	struct iomuxc_gpr_base_regs *iomuxc_gpr_regs = (void *)IOMUXC_GPR_BASE_ADDR;
	u32 gpr1_val = IOMUXC_GPR_GPR1_GPR_ENET_QOS_CLK_GEN_EN;

	if (!device_is_compatible(dev, "nxp,imx8mp-dwmac-eqos"))
		return 0;

	switch (interface_type) {
	case PHY_INTERFACE_MODE_RMII:
		debug("Setting up EQOS in RMII mode\n");
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
				IOMUXC_GPR_GPR1_GPR_ENET_QOS_INTF_SEL_MASK |
				IOMUXC_GPR_GPR1_GPR_ENET_QOS_RGMII_EN,
				IOMUXC_GPR_GPR1_GPR_ENET_QOS_CLK_TX_CLK_SEL |
				gpr1_val | IOMUXC_GPR_GPR1_GPR_ENET_QOS_INTF_SEL_RMII);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		debug("Setting up EQOS in RGMII mode\n");
		/* set INTF as RGMII, enable RGMII TXC clock */
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
				IOMUXC_GPR_GPR1_GPR_ENET_QOS_INTF_SEL_MASK,
				gpr1_val | IOMUXC_GPR_GPR1_GPR_ENET_QOS_INTF_SEL_RGMII |
				IOMUXC_GPR_GPR1_GPR_ENET_QOS_RGMII_EN);
		break;
	default:
		pr_err("Unsupported enet interface type: %s\n",
		       phy_string_for_interface(interface_type));
		return -EINVAL;
	}

	debug("%s@%d: GPR1=%08x\n", __func__, __LINE__,
	      readl(&iomuxc_gpr_regs->gpr[1]));
	if (ret) {
		printf("Failed to set EQOS refclk: %d\n", ret);
		return ret;
	}
	return 0;
}
#endif /* CONFIG_NET */

static struct udevice *thermaldev __section(".data");

#define TEMPERATURE_HOT		80
#define TEMPERATURE_MIN		-40

static void board_show_temp(void)
{
	int ret;
	int cpu_temp;
	static int last_temp = INT_MAX;
	static size_t avg_count;

	ret = uclass_get_device_by_name(UCLASS_THERMAL, "cpu-thermal",
					&thermaldev);
	if (ret) {
		printf("Failed to find THERMAL device: %d\n", ret);
		return;
	}
	if (!thermaldev || thermal_get_temp(thermaldev, &cpu_temp))
		return;

	printf("CPU temperature: %d C\n", cpu_temp);
	if (last_temp == INT_MAX) {
		last_temp = cpu_temp;
	} else if (cpu_temp != last_temp) {
		static int cpu_temps[4] = { -1, };

		if (thermal_get_temp(thermaldev, &cpu_temps[avg_count]))
			return;
		if (++avg_count >= ARRAY_SIZE(cpu_temps)) {
			int bad = -1;
			size_t i;

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

#if IS_ENABLED(CONFIG_DISPLAY_BOARDINFO)
int checkboard(void)
{
#if defined(CONFIG_KARO_TX8P_ML81)
	printf("Board: Ka-Ro TX8P-ML81\n");
#elif defined(CONFIG_KARO_TX8P_ML82)
	printf("Board: Ka-Ro TX8P-ML82\n");
#elif defined(CONFIG_KARO_QSXP_ML81)
	printf("Board: Ka-Ro QSXP-ML81\n");
#else
#error Unsupported module variant
#endif /* CONFIG_KARO_TX8P_ML81 */

	board_show_temp();
	return 0;
}
#endif /* CONFIG_DISPLAY_BOARDINFO */

#if IS_ENABLED(CONFIG_OF_BOARD_FIXUP)
int board_fix_fdt(void *blob)
{
	return 0;
}
#endif

#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				13
#define MIPI				15

#ifdef CONFIG_BOARD_EARLY_INIT_R
int board_early_init_r(void)
{
	int ret;
	struct udevice *cpudev;

	/*
	 * make sure the clktree is correctly initialized via clk_set_defaults()
	 * that is called somewhere down the callchain of uclass_get_device()
	 */
	ret = uclass_get_device(UCLASS_CPU, 0, &cpudev);
	if (ret)
		printf("Failed to get CPU device: %d\n", ret);

	return ret;
}
#endif

int board_init(void)
{
	if (ctrlc())
		printf("<CTRL-C> detected; safeboot enabled\n");

	arm_smccc_smc(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, DISPMIX, true,
		      0, 0, 0, 0, NULL);
	arm_smccc_smc(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN, MIPI, true,
		      0, 0, 0, 0, NULL);

	return 0;
}

int board_late_init(void)
{
	int ret;
	const char *fdt_file;

	karo_env_cleanup();

	if (had_ctrlc()) {
		env_set("safeboot", "1");
		fdt_file = NULL;
	} else {
		fdt_file = env_get("fdt_file");
	}
	if (fdt_file) {
		ret = karo_load_fdt(fdt_file);
		if (ret)
			printf("Failed to load FDT from '%s': %d\n",
			       fdt_file, ret);
	}

	karo_set_ethaddr(0);

	clear_ctrlc();
	return 0;
}

#if IS_ENABLED(CONFIG_ENV_IS_IN_MMC)
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

int ft_board_setup(void *blob, struct bd_info *bd)
{
	return ft_karo_common_setup(blob, bd);
}
