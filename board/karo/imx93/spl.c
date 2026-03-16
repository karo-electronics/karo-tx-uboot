// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Lothar Waßmann <LW@KARO-electronics.de>
 * based on: board/freescale/imx93_evk/spl.c
 *           Copyright 2022 NXP
 */

#include <command.h>
#include <cpu_func.h>
#include <debug_uart.h>
#include <hang.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <malloc.h>
#include <spl.h>
#include <system-constants.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/sections.h>
#include <asm/arch/ccm_regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/ddr.h>
#include <asm/arch/imx93_pins.h>
#include <asm/arch/mu.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/trdc.h>
#include <asm/arch-mx7ulp/gpio.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/ele_api.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <linux/delay.h>
#include <power/pmic.h>
#include <power/pca9450.h>
#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
#define UART_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_DSE(6) | PAD_CTL_FSEL2)

static const iomux_v3_cfg_t uart_pads[] = {
#if CONFIG_DEBUG_UART_BASE == UART1_BASE_ADDR
#define LPUART_CLK	LPUART1_CLK_ROOT
	MX93_PAD_UART1_RXD__LPUART1_RX | UART_PAD_CTRL,
	MX93_PAD_UART1_TXD__LPUART1_TX | UART_PAD_CTRL,
#elif CONFIG_DEBUG_UART_BASE == UART2_BASE_ADDR
#define LPUART_CLK	LPUART2_CLK_ROOT
	MX93_PAD_UART2_RXD__LPUART2_RX | UART_PAD_CTRL,
	MX93_PAD_UART2_TXD__LPUART2_TX | UART_PAD_CTRL,
#elif CONFIG_DEBUG_UART_BASE == UART4_BASE_ADDR
#define LPUART_CLK	LPUART4_CLK_ROOT
	MX93_PAD_ENET2_RD0__LPUART4_RX | UART_PAD_CTRL,
	MX93_PAD_ENET2_TD0__LPUART4_TX | UART_PAD_CTRL,
#elif CONFIG_DEBUG_UART_BASE == UART5_BASE_ADDR
#define LPUART_CLK	LPUART5_CLK_ROOT
	MX93_PAD_DAP_TDI__LPUART5_RX | UART_PAD_CTRL,
	MX93_PAD_DAP_TDO_TRACESWO__LPUART5_TX | UART_PAD_CTRL,
#elif CONFIG_DEBUG_UART_BASE == UART8_BASE_ADDR
#define LPUART_CLK	LPUART8_CLK_ROOT
	MX93_PAD_GPIO_IO13__LPUART8_RX | UART_PAD_CTRL,
	MX93_PAD_GPIO_IO12__LPUART8_TX | UART_PAD_CTRL,
#else
#error Unsupported DEBUG_UART_BASE
#endif
};

void board_debug_uart_init(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

	init_uart_clk(LPUART_CLK);
}
#endif

#define WDOG_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_DSE(6) | PAD_CTL_ODE | PAD_CTL_PUE)

static const iomux_v3_cfg_t wdog_pads[] = {
	MX93_PAD_WDOG_ANY__WDOG1_WDOG_ANY | WDOG_PAD_CTRL,
};

void __noreturn jump_to_image_no_args(struct spl_image_info *spl_image)
{
	typedef void __noreturn (*image_entry_noargs_t)(void);
	u32 __maybe_unused offset;
	const void *fdt_addr = gd->fdt_blob;

	image_entry_noargs_t image_entry =
		(image_entry_noargs_t)(uintptr_t)spl_image->entry_point;
#ifdef DEBUG
	uintptr_t sp;

	asm("\tmov	%0, sp\n" : "=r"(sp));
	debug("image entry point: 0x%p sp=%08lx\n", image_entry, sp);
	debug("fdtaddr=%p\n", fdt_addr);
#if CONFIG_IS_ENABLED(SYS_MALLOC_F)
	malloc_simple_info();
	printf("@%08lx..%08lx\n", gd->malloc_base,
	       gd->malloc_base + gd->malloc_ptr - 1);
#endif /* SYS_MALLOC_F */
#endif /* DEBUG */

#if CONFIG_IS_ENABLED(IMX_HAB)
	/*
	 * HAB looks for the CSF at the end of the authenticated
	 * data, therefore we need to subtract the size of the
	 * CSF from the actual filesize
	 */
	offset = spl_image->size - CONFIG_CSF_SIZE;
	if (!imx_hab_authenticate_image(spl_image->load_addr,
					offset + IVT_SIZE +
					CSF_PAD_SIZE, offset)) {
		asm("\tmov x1, %0\n"
		    :: "r"(fdt_addr) : "x0", "x1", "x2", "x3");
		image_entry();
	} else {
		panic("spl: ERROR: failed to authenticate bootloader image\n");
	}
#endif

	asm("\tmov x1, %0\n"
	    :: "r"(fdt_addr) : "x0", "x1", "x2", "x3");
	image_entry();
}

int spl_board_boot_device(enum boot_device boot_dev_spl)
{
	return BOOT_DEVICE_BOOTROM;
}

static inline void spl_wdog_init(void)
{
	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));
}

void spl_board_init(void)
{
	int ret;

	ret = ele_start_rng();
	if (ret)
		printf("Failed to start RNG: %d\n", ret);

	puts("Normal Boot\n");
}

void spl_dram_init(void)
{
	int loops = 0;
	int max_loops = 5;

	while (ddr_init(&dram_timing)) {
		if (loops == 0)
			printf("Retrying...\n");
		if (loops < max_loops)
			loops++;
		else
			panic("DDR Training FAILED\n");
	}
	if (loops)
		printf("DDR Training OK after %u loops\n", loops);
}

static inline void __pmic_reg_write(struct udevice *dev, unsigned int reg,
				    u8 val, const char *fn, int ln)
{
	int ret;

	ret = pmic_reg_read(dev, reg);
	if (ret < 0) {
		printf("%s@%d: Failed to read pmic reg %02x: %d\n", fn, ln, reg, ret);
		return;
	}

	if (ret == val) {
		debug("%s@%d: Leaving pmic reg %02x at %02x\n", fn, ln, reg, ret);
		return;
	}
	debug("%s@%d: Changing pmic reg %02x from %02x to %02x\n", fn, ln, reg, ret, val);

	ret = pmic_reg_write(dev, reg, val);
	if (ret == 0)
		debug("%s@%d: wrote %02x to pmic reg %02x\n", fn, ln, val, reg);
	else
		printf("%s@%d: failed to write %02x to pmic reg %02x: %d\n", fn, ln,
		       val, reg, ret);
}

#define pmic_reg_write(d, r, v)	__pmic_reg_write(d, r, v, __func__, __LINE__)

#if CONFIG_IS_ENABLED(DM_PMIC_PCA9450)
int power_init_board(void)
{
	struct udevice *dev;
	int ret;

	ret = pmic_get("pmic@25", &dev);
	if (ret == -ENODEV) {
		puts("No pmic@25\n");
		return 0;
	}
	if (ret != 0)
		return ret;

	/* reset all register to default values */
	pmic_reg_write(dev, PCA9450_SW_RST, 0x05);

	/* Disable TOFF_DEB */
	pmic_reg_write(dev, PCA9450_PWR_CTRL, 0x4c);

	/* 0.8625v for Overdrive mode */
	pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS0, 0x15);
	pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS1, 0x01);

	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	pmic_reg_write(dev, PCA9450_BUCK123_DVS, 0x29);

	/* enable DVS control through PMIC_STBY_REQ */
	pmic_reg_write(dev, PCA9450_BUCK1CTRL, 0x59);

	/* disable BUCK2 and activate discharge resistor */
	pmic_reg_write(dev, PCA9450_BUCK2CTRL, 0x48);

	/* switch off unused LDOs */
	pmic_reg_write(dev, PCA9450_LDO2CTRL, 0x00);
	pmic_reg_write(dev, PCA9450_LDO3CTRL, 0x00);
	pmic_reg_write(dev, PCA9450_LDO5CTRL_L, 0x00);

	/* set WDOG_B_CFG to cold reset */
	pmic_reg_write(dev, PCA9450_RESET_CTRL, 0xa1);
	return 0;
}
#endif

struct mem_region {
	const char *name;
	unsigned long start;
	unsigned long end;
	u32 flags;
};

#if !IS_ENABLED(CONFIG_SPL_FRAMEWORK_BOARD_INIT_F)
#if IS_ENABLED(CONFIG_KARO_UBOOT_MFG) || defined(DEBUG)

#define BL31_END	(CONFIG_BL31_BASE + CONFIG_BL31_SIZE)

enum mem_regions {
	SPL,
	DTB,
	DDRFW,
	DRAMCFG,
	BSS,
	STACK,
	MALLOC,
	BL31,
	RSRVD,
#if IS_ENABLED(CONFIG_PRE_CONSOLE_BUFFER)
	PRE_CON_BUF,
#endif
};

#ifdef CONFIG_SPL_BSS_MAX_SIZE
#define BSS_SIZE CONFIG_SPL_BSS_MAX_SIZE
#else
#define BSS_SIZE	((uintptr_t)&__bss_end - (uintptr_t)&__bss_start)
#endif

static struct mem_region iram_regions[] = {
	{ "TCM", 0x0ffc0000, 0x0ffe0000, },
	{ "OCRAM", 0x20480000, 0x20520000, },
};

#define SHARED_REGION(n)		BIT(n)

static struct mem_region mem_regions[] = {
	[SPL] = { "SPL", CONFIG_SPL_TEXT_BASE, (uintptr_t)&_end, },
	[DTB] = { "DTB", (uintptr_t)&_end, },
	[DDRFW] = { "DDRFW", 0, 0, SHARED_REGION(0), },
	[BSS] = { "BSS", (uintptr_t)&__bss_start, (uintptr_t)&__bss_start + BSS_SIZE, },
	[STACK] = { "STACK+GD", },
	[MALLOC] = { "MALLOC", },
	[BL31] = { "BL31", CONFIG_BL31_BASE, BL31_END, SHARED_REGION(0), },
	[DRAMCFG] = {"DRAMCFG", CONFIG_SAVED_DRAM_TIMING_BASE, },
	[RSRVD] = { "**RSRVD**", 0x20484000, 0x20488000, },
#if IS_ENABLED(CONFIG_PRE_CONSOLE_BUFFER)
	[PRE_CON_BUF] = {"RS232BUF", CONFIG_PRE_CON_BUF_ADDR,
			 CONFIG_PRE_CON_BUF_ADDR + CONFIG_PRE_CON_BUF_SZ, },
#endif
};

static const size_t num_regions = ARRAY_SIZE(mem_regions);

static int region_is_in_iram(const struct mem_region *r1, const struct mem_region *ri)
{
	size_t overflow;

	if (r1->start >= ri->start && r1->end <= ri->end)
		return 0;
	if (r1->end <= ri->start || r1->start >= ri->end)
		return 1;
	overflow = r1->end > ri->end ? r1->end - ri->end : 0;
	overflow += r1->start < ri->start ? ri->start - r1->start : 0;
	printf("%-8s:\t%08lx..%08lx (%08lx) overflows %-8s %08lx..%08lx by %zu (%08zx) bytes\n",
	       r1->name, r1->start, r1->end - 1, r1->end - r1->start,
	       ri->name, ri->start, ri->end - 1, overflow, overflow);
	return -1;
}

static bool check_region(const struct mem_region *r1, const struct mem_region *r2)
{
	size_t overlap;

	if (!r1)
		return true;

	if (r1->start >= r2->end || r1->end <= r2->start)
		return true;

	if (r2->start >= r1->start)
		overlap = min(r1->end - r2->start, r2->end - r2->start);
	else if (r1->start > r2->start)
		overlap = min(r2->end - r1->start, r2->end - r2->start);
	if (r1->flags | r2->flags && r1->flags == r2->flags)
		printf("%-8s:\t%08lx..%08lx (%08lx) shares %8s %08lx..%08lx for %zu (%08zx) bytes\n",
		       r2->name, r2->start, r2->end - 1, r2->end - r2->start,
		       r1->name, r1->start, r1->end - 1, overlap, overlap);
	else
		printf("%-8s:\t%08lx..%08lx (%08lx) overlaps %8s %08lx..%08lx by %zu (%08zx) bytes\n",
		       r2->name, r2->start, r2->end - 1, r2->end - r2->start,
		       r1->name, r1->start, r1->end - 1, overlap, overlap);

	return false;
}

#ifdef CONFIG_SPL_SIZE_LIMIT_PROVIDE_STACK
#define STACK_SIZE	CONFIG_VAL(SIZE_LIMIT_PROVIDE_STACK)
#else
#define STACK_SIZE	SZ_8K
#endif

#if !defined(CFG_MALLOC_F_ADDR)
#define MALLOC_START_ADDR	get_spl_stack()
#else
#define MALLOC_START_ADDR	CFG_MALLOC_F_ADDR
#endif

static inline unsigned long get_spl_stack(void)
{
	unsigned long spl_stack;

#if CONFIG_IS_ENABLED(HAVE_INIT_STACK)
	spl_stack = CONFIG_SPL_STACK;
#elif IS_ENABLED(CONFIG_INIT_SP_RELATIVE)
	spl_stack = (unsigned long)&__bss_start + CONFIG_SYS_INIT_SP_BSS_OFFSET;
#else
	spl_stack = SYS_INIT_SP_ADDR;
#endif
	spl_stack = rounddown(spl_stack, 16);
#ifndef CFG_MALLOC_F_ADDR
	if (CONFIG_IS_ENABLED(SYS_MALLOC_F)) {
		spl_stack -= CONFIG_VAL(SYS_MALLOC_F_LEN);
	}
#endif
	return spl_stack;
}

static inline unsigned long dram_timing_size(struct dram_timing_info *dram_timing)
{
	return dram_timing->ddrc_cfg_num * sizeof(*dram_timing->ddrc_cfg) +
		dram_timing->ddrphy_cfg_num * sizeof(*dram_timing->ddrphy_cfg) +
		dram_timing->fsp_msg_num * sizeof(*dram_timing->fsp_msg) +
		dram_timing->ddrphy_trained_csr_num * sizeof(*dram_timing->ddrphy_trained_csr) +
		dram_timing->ddrphy_pie_num * sizeof(*dram_timing->ddrphy_pie) +
		dram_timing->fsp_cfg_num * sizeof(*dram_timing->fsp_cfg) +
		sizeof(*dram_timing->fsp_table);
}

static inline unsigned long ddrfw_size(void)
{
	unsigned long ddrfw_size = 0;

	if (BINMAN_SYMS_OK) {
		ddrfw_size += binman_sym(ulong, ddr_1d_imem_fw, size);
		ddrfw_size += binman_sym(ulong, ddr_1d_dmem_fw, size);
#if !IS_ENABLED(CONFIG_IMX8M_DDR3L)
		ddrfw_size += binman_sym(ulong, ddr_2d_imem_fw, size);
		ddrfw_size += binman_sym(ulong, ddr_2d_dmem_fw, size);
#endif
	} else {
		ddrfw_size = SPL_DDRFW_SIZE;
	}
	return ddrfw_size;
}

static void check_mem_regions(struct mem_region *mem_regions, size_t num_regions)
{
	uintptr_t sp;
	uintptr_t eof;
	uintptr_t dtb = (uintptr_t)gd->fdt_blob;
	size_t i, j;
	int err = 0;
	size_t mri[num_regions];

	if (CONFIG_IS_ENABLED(SEPARATE_BSS))
		eof = (uintptr_t)&_end;
	else
		eof = (uintptr_t)&__bss_end;

	memset(mri, 0xff, sizeof(mri));

	asm("mov %0, sp\n" : "=r"(sp));

	mem_regions[DRAMCFG].end = mem_regions[DRAMCFG].start + dram_timing_size(&dram_timing);
	mem_regions[STACK].end = get_spl_stack();
	mem_regions[STACK].start = mem_regions[STACK].end - STACK_SIZE - ALIGN(GD_SIZE, 16);

	if (sp < mem_regions[STACK].start) {
		printf("Stack overflow: sp=%08lx [%08lx..%08lx]\n", sp,
		       mem_regions[STACK].start, mem_regions[STACK].end - 1);
		err++;
	}
	if (sp >= mem_regions[STACK].end) {
		printf("Stack underflow: sp=%08lx [%08lx..%08lx]\n", sp,
		       mem_regions[STACK].start, mem_regions[STACK].end - 1);
		err++;
	}

	if (gd->fdt_blob && !fdt_check_header(gd->fdt_blob)) {
		size_t fdtalign = ALIGN(fdt_totalsize(gd->fdt_blob), 4);

		eof += fdtalign;
		mem_regions[DTB].start = dtb;
		mem_regions[DTB].end = dtb + fdtalign;
	} else {
		printf("No valid DTB found\n");
	}
	mem_regions[DDRFW].start = eof;
	mem_regions[DDRFW].end = eof + ddrfw_size();
#if CONFIG_IS_ENABLED(SYS_MALLOC_F)
	mem_regions[MALLOC].start = MALLOC_START_ADDR;
	mem_regions[MALLOC].end = MALLOC_START_ADDR + CONFIG_VAL(SYS_MALLOC_F_LEN);
#endif

	for (i = j = 0; i < num_regions; i++) {
		int k;

		if (!mem_regions[i].start || !mem_regions[i].end)
			continue;

		mri[j++] = i;
		if (i == 0)
			continue;
		for (k = j; k > 0; k--) {
			if (mem_regions[mri[k - 1]].start < mem_regions[i].start)
				break;

			mri[k] = mri[k - 1];
			mri[k - 1] = i;
		}
	}

	for (i = 0; i < ARRAY_SIZE(iram_regions); i++) {
		struct mem_region *ri = &iram_regions[i];
		struct mem_region *rl = NULL;
		size_t k;

		printf("\n%-8s:\t%08lx..%08lx\n", ri->name, ri->start, ri->end - 1);
		for (k = 0; k < j; k++) {
			struct mem_region *r1 = NULL;
			struct mem_region *r2;
			int err;

			if (mri[k] < 0)
				break;
			r2 = &mem_regions[mri[k]];
			err = region_is_in_iram(r2, ri);
			if (err)
				continue;
			if (k > 0) {
				r1 = &mem_regions[mri[k - 1]];
				if (r2->start > r1->end && rl && r2->start > rl->end)
					printf("%-8s:\t%08lx..%08lx (%08lx)\n", " **GAP**",
					       r1->end, r2->start - 1,
					       r2->start - r1->end);
			}
			if (check_region(r1, r2)) {
				printf("%-8s:\t%08lx..%08lx (%08lx)\n", r2->name,
				       r2->start, r2->end - 1,
				       r2->end - r2->start);
				rl = r2;
			} else {
				err++;
			}
		}
		if (!rl)
			printf("%-8s:\t%08lx..%08lx (%08lx)\n", " **GAP**",
			       ri->start, ri->end - 1,
			       ri->end - ri->start);
		else if (ri->end > rl->end)
			printf("%-8s:\t%08lx..%08lx (%08lx)\n", " **GAP**",
			       rl->end, ri->end - 1,
			       ri->end - rl->end);
	}
	if (!err)
		puts("\n");
	else
#if defined(DEBUG)
		puts("Memory regions overlap detected\n");
#else
		panic("Memory regions overlap detected\n");
#endif
}
#else
static struct mem_region *mem_regions;
static const size_t num_regions;
static inline void check_mem_regions(struct mem_region *mem_regions, size_t num_regions)
{
}
#endif /* CONFIG_KARO_UBOOT_MFG || DEBUG */

void board_init_f(ulong dummy)
{
	int ret;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	spl_wdog_init();

	timer_init();

	arch_cpu_init();

	spl_early_init();

	preloader_console_init();

	check_mem_regions(mem_regions, num_regions);

	ret = imx9_probe_mu();
	if (ret) {
		printf("Failed to init ELE API: %d\n", ret);
	} else {
		debug("SOC: 0x%x\n", gd->arch.soc_rev);
		debug("LC: 0x%x\n", gd->arch.lifecycle);
	}

	clock_init_late();

	power_init_board();

	if (!is_voltage_mode(VOLT_LOW_DRIVE))
		set_arm_core_max_clk();

	/* Init power of mix */
	soc_power_init();

	/* Setup TRDC for DDR access */
	trdc_init();

	/* DDR initialization */
	spl_dram_init();

	/* Put M33 into CPUWAIT for following kick */
	ret = m33_prepare();
	if (!ret)
		printf("M33 prepare ok\n");

	board_init_r(NULL, 0);
}
#endif
