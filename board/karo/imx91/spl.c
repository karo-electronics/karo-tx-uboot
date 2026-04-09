// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Marcel Mertens <MM@KARO-electronics.de>
 * based on: board/freescale/imx91_evk/spl.c
 *           Copyright 2024 NXP
 */

#include <command.h>
#include <cpu_func.h>
#include <debug_uart.h>
#include <hang.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <spl.h>
#include <system-constants.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/sections.h>
#include <asm/arch/ccm_regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/ddr.h>
#include <asm/arch/imx91_pins.h>
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

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
#define UART_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_DSE(6) | PAD_CTL_FSEL2)

static const iomux_v3_cfg_t uart_pads[] = {
#if CONFIG_DEBUG_UART_BASE == UART1_BASE_ADDR
#define LPUART_CLK	LPUART1_CLK_ROOT
	MX91_PAD_UART1_RXD__LPUART1_RX | UART_PAD_CTRL,
	MX91_PAD_UART1_TXD__LPUART1_TX | UART_PAD_CTRL,
#elif CONFIG_DEBUG_UART_BASE == UART2_BASE_ADDR
#define LPUART_CLK	LPUART2_CLK_ROOT
	MX91_PAD_UART2_RXD__LPUART2_RX | UART_PAD_CTRL,
	MX91_PAD_UART2_TXD__LPUART2_TX | UART_PAD_CTRL,
#elif CONFIG_DEBUG_UART_BASE == UART4_BASE_ADDR
#define LPUART_CLK	LPUART4_CLK_ROOT
	MX91_PAD_ENET2_RD0__LPUART4_RX | UART_PAD_CTRL,
	MX91_PAD_ENET2_TD0__LPUART4_TX | UART_PAD_CTRL,
#elif CONFIG_DEBUG_UART_BASE == UART5_BASE_ADDR
#define LPUART_CLK	LPUART5_CLK_ROOT
	MX91_PAD_DAP_TDI__LPUART5_RX | UART_PAD_CTRL,
	MX91_PAD_DAP_TDO_TRACESWO__LPUART5_TX | UART_PAD_CTRL,
#elif CONFIG_DEBUG_UART_BASE == UART8_BASE_ADDR
#define LPUART_CLK	LPUART8_CLK_ROOT
	MX91_PAD_GPIO_IO13__LPUART8_RX | UART_PAD_CTRL,
	MX91_PAD_GPIO_IO12__LPUART8_TX | UART_PAD_CTRL,
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
	MX91_PAD_WDOG_ANY__WDOG1_WDOG_ANY | WDOG_PAD_CTRL,
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

	/* Enable WDOG resets */
	clrbits_le32(SRC_BASE_ADDR + 0x18, 0x1c);
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

	/* 0.8125v for Overdrive mode */
	pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS0, 0x11);
	pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS1, 0x01);

	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	pmic_reg_write(dev, PCA9450_BUCK123_DVS, 0x78);

	/* enable DVS control through PMIC_STBY_REQ */
	pmic_reg_write(dev, PCA9450_BUCK1CTRL, 0x59);

	/* disable BUCK2 and activate discharge resistor */
	pmic_reg_write(dev, PCA9450_BUCK2CTRL, 0x48);

	/* switch off unused LDOs */
	pmic_reg_write(dev, PCA9450_LDO2CTRL, 0x00);
	pmic_reg_write(dev, PCA9450_LDO3CTRL, 0x00);
	pmic_reg_write(dev, PCA9450_LDO5CTRL_L, 0x00);

	/* set WDOG_B_CFG to cold reset */
	pmic_reg_write(dev, PCA9450_RESET_CTRL, 0x61);
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
	RSRVD1,
	RSRVD2,
#if IS_ENABLED(CONFIG_PRE_CONSOLE_BUFFER)
	PRE_CON_BUF,
#endif
};

#define SHARED_REGION(n)		BIT(n)

#define BSS_SIZE	((uintptr_t)&__bss_end - (uintptr_t)&__bss_start)

static struct mem_region mem_regions[] = {
	[SPL] = { "SPL", CONFIG_SPL_TEXT_BASE, (uintptr_t)&_end, },
	[DTB] = { "DTB", (uintptr_t)&_end, },
	[DDRFW] = { "DDRFW", 0, 0, SHARED_REGION(1), },
	[BSS] = { "BSS", (uintptr_t)&__bss_start,
		  (uintptr_t)&__bss_start + BSS_SIZE, },
	[STACK] = { "STACK+GD", },
	[MALLOC] = { "MALLOC", },
	[BL31] = { "BL31", CONFIG_BL31_BASE, BL31_END, SHARED_REGION(1), },
	[DRAMCFG] = {"DRAMCFG", CONFIG_SAVED_DRAM_TIMING_BASE, },
	[RSRVD1] = {"**RSRVD**", 0x20480000, 0x20486000, },
	[RSRVD2] = {"**RSRVD**", 0x2048f800, 0x20490000, },
#if IS_ENABLED(CONFIG_PRE_CONSOLE_BUFFER)
	[PRE_CON_BUF] = {"RS232BUF", CONFIG_PRE_CON_BUF_ADDR,
			 CONFIG_PRE_CON_BUF_ADDR + CONFIG_PRE_CON_BUF_SZ, },
#endif
};

static const size_t num_regions = ARRAY_SIZE(mem_regions);

static bool check_region(const struct mem_region *r1, const struct mem_region *r2)
{
	size_t overlap;

	if (r1->start >= r2->end || r1->end <= r2->start)
		return true;

	if (r1->start <= r2->start)
		overlap = min(r1->end - r2->start, r1->end - r1->start);
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
#define MALLOC_START_ADDR	SPL_STACK_END
#else
#define MALLOC_START_ADDR	CFG_MALLOC_F_ADDR
#endif

#if CONFIG_IS_ENABLED(HAVE_INIT_STACK)
#if CONFIG_IS_ENABLED(SYS_MALLOC_F) && !defined(CFG_MALLOC_F_ADDR)
#define SPL_STACK_END		(CONFIG_VAL(STACK) - CONFIG_VAL(SYS_MALLOC_F_LEN))
#else
#define SPL_STACK_END		CONFIG_VAL(STACK)
#endif
#else
#if !defined(CFG_MALLOC_F_ADDR)
#define SPL_STACK_END		(SYS_INIT_SP_ADDR - CONFIG_VAL(SYS_MALLOC_F_LEN))
#else
#define SPL_STACK_END		SYS_INIT_SP_ADDR
#endif
#endif

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

static void check_mem_regions(struct mem_region *mem_regions, size_t num_regions)
{
	uintptr_t sp;
	uintptr_t eof;
	uintptr_t dtb = (uintptr_t)gd->fdt_blob;
	size_t i, j;
	int err = 0;
	int mri[num_regions];

	if (CONFIG_IS_ENABLED(SEPARATE_BSS))
		eof = (uintptr_t)&_end;
	else
		eof = (uintptr_t)&__bss_end;

	memset(mri, 0xff, sizeof(mri));
	const uintptr_t iram_start = 0x20480000;
	const uintptr_t iram_end = iram_start + 384 * SZ_1K;

	asm("mov %0, sp\n" : "=r"(sp));

	mem_regions[STACK].end = SPL_STACK_END;
	mem_regions[STACK].start = SPL_STACK_END - STACK_SIZE - ALIGN(GD_SIZE, 16);
	mem_regions[DRAMCFG].end = mem_regions[DRAMCFG].start + dram_timing_size(&dram_timing);

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
	mem_regions[DDRFW].end = eof + SPL_DDRFW_SIZE;
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
	for (i = 0; i < j; i++) {
		struct mem_region *r1 = NULL;
		struct mem_region *r2;

		if (mri[i] < 0)
			break;
		r2 = &mem_regions[mri[i]];
		if (i > 0) {
			r1 = &mem_regions[mri[i - 1]];
			if (r1->end < r2->start)
				printf("%-8s:\t%08lx..%08lx (%08lx)\n", " **GAP**",
				       r1->end, r2->start - 1,
				       r2->start - r1->end);
		} else {
			if (iram_start < r2->start)
				printf("%-8s:\t%08lx..%08lx (%08lx)\n", " **GAP**",
				       iram_start, r2->start - 1,
				       r2->start - iram_start);
		}
		if (r2->end <= iram_end && r2->start >= iram_start) {
			if (check_region(r1, r2))
				printf("%-8s:\t%08lx..%08lx (%08lx)\n", r2->name,
				       r2->start, r2->end - 1,
				       r2->end - r2->start);
			else
				err = r1->flags | r2->flags && r1->flags != r2->flags;
		} else if (r2->start >= iram_end ||
			   r2->end < iram_start) {
			printf("%-8s:\t%08lx..%08lx (%08lx) is outside IRAM:\t%08lx..%08lx\n",
			       r2->name, r2->start,
			       r2->end, r2->end - r2->start,
			       iram_start, iram_end - 1);
		} else {
			if (r2->end < iram_end)
				printf("%-8s:\t%08lx..%08lx (%08lx) overflows IRAM:\t%08lx..%08lx by %lu (%08lx) bytes\n",
				       r2->name, r2->start,
				       r2->end, r2->end - r2->start,
				       iram_start, iram_end - 1,
				       iram_end - r2->end, iram_end - r2->end);
			else
				printf("%-8s:\t%08lx..%08lx (%08lx) overflows IRAM:\t%08lx..%08lx by %lu (%08lx) bytes\n",
				       r2->name, r2->start,
				       r2->end, r2->end - r2->start,
				       iram_start, iram_end - 1,
				       r2->end - iram_end, r2->end - iram_end);
			err = 1;
		}
		if (i == ARRAY_SIZE(mri) - 1) {
			if (r2->end < iram_end)
				printf("%-8s:\t%08lx..%08lx (%08lx)\n", " **GAP**",
				       r2->end, iram_end - 1,
				       iram_end - r2->end);
		}
	}
	if (err)
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

	board_init_r(NULL, 0);
}
#endif
