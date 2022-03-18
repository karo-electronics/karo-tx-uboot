#include <common.h>
#include <console.h>
#include <cpu_func.h>
#include <image.h>
#include <init.h>
#include <malloc.h>
#include <netdev.h>
#include <dm.h>
#include <dm/platform_data/serial_sh.h>
#include <asm/processor.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/arch/gpio.h>
#include <asm/arch/rmobile.h>
#include <asm/arch/rcar-mstp.h>
#include <asm/arch/sh_sdhi.h>
#include <i2c.h>
#include <mmc.h>

#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

#define PFC_BASE	0x11030000

#define ETH_CH0		(PFC_BASE + 0x300c)
#define ETH_CH1		(PFC_BASE + 0x3010)
#define I2C_CH1 	(PFC_BASE + 0x1870)
#define ETH_PVDD_3300	0x00
#define ETH_PVDD_1800	0x01
#define ETH_PVDD_2500	0x02
#define ETH_MII_RGMII	(PFC_BASE + 0x3018)
#define ENABLE_MII	0x03

/* ETH */
#define ETH_BASE 	0x11c20000
#define CXR35 		(ETH_BASE + 0x540)

/* CPG */
#define CPG_BASE				0x11010000
#define CPG_CLKON_BASE				(CPG_BASE + 0x500)
#define CPG_RST_BASE				(CPG_BASE + 0x800)
#define CPG_RST_ETH				(CPG_RST_BASE + 0x7C)
#define CPG_RESET_I2C                           (CPG_RST_BASE + 0x80)
#define CPG_PL2_SDHI_DSEL                       (CPG_BASE + 0x218)
#define CPG_CLK_STATUS                          (CPG_BASE + 0x280)

void s_init(void)
{
	/* can go in board_eht_init() once enabled */
	writel(ETH_PVDD_1800, ETH_CH0);
	writel(ETH_PVDD_1800, ETH_CH1);
	/* Enable MII for both ETH{0,1} */
	writel(ENABLE_MII, ETH_MII_RGMII);
	/* ETH RST */
	writel(0x30003, CPG_RST_ETH);
	/* I2C CLK */
	writel(0xF000F, CPG_RESET_I2C);
	/* I2C pin non GPIO enable */
	writel(0x01010101, I2C_CH1);
	/* SD CLK */
	writel(0x00110011, CPG_PL2_SDHI_DSEL);
	while (*(volatile u32 *)(CPG_CLK_STATUS) != 0)
		;
}

int board_early_init_f(void)
{
	return 0;
}

int board_init(void)
{
	/* adress of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_TEXT_BASE + 0x50000;

	return 0;
}

int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	const void *fdt_compat;
	ofnode root = ofnode_path("/");

	fdt_compat = ofnode_read_string(root, "compatible");

	if (fdt_compat) {
		if (strncmp(fdt_compat, "karo,", 5) != 0)
			env_set("board_name", fdt_compat);
		else
			env_set("board_name", fdt_compat + 5);
	}
#endif
	env_cleanup();

	if (had_ctrlc()) {
		env_set_hex("safeboot", 1);
	} else if (!IS_ENABLED(CONFIG_KARO_UBOOT_MFG)) {
		karo_fdt_move_fdt();
	}

	clear_ctrlc();
	return 0;
}

void reset_cpu(ulong addr)
{

}
