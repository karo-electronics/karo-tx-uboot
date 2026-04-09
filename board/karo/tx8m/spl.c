// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <errno.h>
#include <fdtdec.h>
#include <fsl_esdhc_imx.h>
#include <hang.h>
#include <i2c.h>
#include <init.h>
#include <malloc.h>
#include <mmc.h>
#include <spl.h>
#include <system-constants.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#if defined(CONFIG_IMX8MM)
#include <asm/arch/imx8mm_pins.h>
#elif defined(CONFIG_IMX8MN)
#include <asm/arch/imx8mn_pins.h>
#elif defined(CONFIG_IMX8MP)
#include <asm/arch/imx8mp_pins.h>
#else
#error Invalid SOC type selection
#endif
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/sections.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/ofnode.h>
#include <dm/read.h>
#include <power/pmic.h>
#include <asm/arch/ddr.h>
#include "pmic.h"

#ifdef CONFIG_DEBUG_UART
#include <debug_uart.h>
#ifdef CONFIG_DEBUG_UART_BOARD_INIT
#define debug_uart_init() do {} while (0)
#endif
#else
#define debug_uart_init() do {} while (0)
#define printascii(v) do {} while (0)
#define printhex2(v) do {} while (0)
#endif

DECLARE_GLOBAL_DATA_PTR;

void __noreturn jump_to_image_no_args(struct spl_image_info *spl_image)
{
	typedef void __noreturn (*image_entry_noargs_t)(void);
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

	asm("\tmov x1, %0\n"
	    :: "r"(fdt_addr) : "x0", "x1", "x2", "x3");
	image_entry();
}

#if IS_ENABLED(CONFIG_DEBUG_UART_BOARD_INIT)
#define UART_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_FSEL1 |	\
					     PAD_CTL_DSE6)
static const iomux_v3_cfg_t uart_pads[] = {
#if CONFIG_DEBUG_UART_BASE == UART1_BASE_ADDR
#define UART_IDX 0
#if defined(CONFIG_IMX8MM)
	IMX8MM_PAD_UART1_RXD_UART1_RX | UART_PAD_CTRL,
	IMX8MM_PAD_UART1_TXD_UART1_TX | UART_PAD_CTRL,
#elif defined(CONFIG_IMX8MN)
	IMX8MN_PAD_UART1_RXD__UART1_DCE_RX | UART_PAD_CTRL,
	IMX8MN_PAD_UART1_TXD__UART1_DCE_TX | UART_PAD_CTRL,
#elif defined(CONFIG_IMX8MP)
	MX8MP_PAD_UART1_RXD__UART1_DCE_RX | UART_PAD_CTRL,
	MX8MP_PAD_UART1_TXD__UART1_DCE_TX | UART_PAD_CTRL,
#endif
#elif CONFIG_DEBUG_UART_BASE == UART2_BASE_ADDR
#define UART_IDX 1
#if defined(CONFIG_IMX8MM)
	IMX8MM_PAD_UART2_RXD_UART2_RX | UART_PAD_CTRL,
	IMX8MM_PAD_UART2_TXD_UART2_TX | UART_PAD_CTRL,
#elif defined(CONFIG_IMX8MN)
	IMX8MN_PAD_UART2_RXD__UART2_DCE_RX | UART_PAD_CTRL,
	IMX8MN_PAD_UART2_TXD__UART2_DCE_TX | UART_PAD_CTRL,
#elif defined(CONFIG_IMX8MP)
	MX8MP_PAD_UART2_RXD__UART2_DCE_RX | UART_PAD_CTRL,
	MX8MP_PAD_UART2_TXD__UART2_DCE_TX | UART_PAD_CTRL,
#endif
#elif CONFIG_DEBUG_UART_BASE == UART3_BASE_ADDR
#define UART_IDX 2
#if defined(CONFIG_IMX8MM)
#if defined(CONFIG_KARO_QS8M)
	IMX8MM_PAD_UART3_RXD_UART3_RX | UART_PAD_CTRL,
	IMX8MM_PAD_UART3_TXD_UART3_TX | UART_PAD_CTRL,
#else
	IMX8MM_PAD_ECSPI1_MOSI_UART3_RX | UART_PAD_CTRL,
	IMX8MM_PAD_ECSPI1_SCLK_UART3_TX | UART_PAD_CTRL,
#endif
#elif defined(CONFIG_IMX8MN)
	IMX8MN_PAD_UART3_RXD__UART3_DCE_RX | UART_PAD_CTRL,
	IMX8MN_PAD_UART3_TXD__UART3_DCE_TX | UART_PAD_CTRL,
#elif defined(CONFIG_IMX8MP)
	MX8MP_PAD_UART3_RXD__UART3_DCE_RX | UART_PAD_CTRL,
	MX8MP_PAD_UART3_TXD__UART3_DCE_TX | UART_PAD_CTRL,
#endif
#elif CONFIG_DEBUG_UART_BASE == UART4_BASE_ADDR
#define UART_IDX 3
#if defined(CONFIG_IMX8MM)
	IMX8MM_PAD_UART4_TXD_UART4_RX | UART_PAD_CTRL,
	IMX8MM_PAD_UART4_RXD_UART4_TX | UART_PAD_CTRL,
#elif defined(CONFIG_IMX8MN)
	IMX8MN_PAD_UART4_RXD__UART4_DCE_RX | UART_PAD_CTRL,
	IMX8MN_PAD_UART4_TXD__UART4_DCE_TX | UART_PAD_CTRL,
#elif defined(CONFIG_IMX8MP)
	MX8MP_PAD_UART4_RXD__UART4_DCE_RX | UART_PAD_CTRL,
	MX8MP_PAD_UART4_TXD__UART4_DCE_TX | UART_PAD_CTRL,
#endif
#else
#error unsupported UART selected with CONFIG_DEBUG_UART_BASE
#endif /* CONFIG_DEBUG_UART_BASE == */
};
#endif /* CONFIG_DEBUG_UART_BOARD_INIT */

/* called before debug_uart is initialized */
#ifdef CONFIG_IMX8MP
#define WDOG_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_DSE6 | PAD_CTL_PUE | PAD_CTL_PE)

static const iomux_v3_cfg_t wdog_pads[] = {
	MX8MP_PAD_GPIO1_IO02__WDOG1_WDOG_B  | WDOG_PAD_CTRL,
};

static void spl_wdog_init(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);
}
#else
static inline void spl_wdog_init(void)
{
}
#endif

#if IS_ENABLED(CONFIG_DEBUG_UART_BOARD_INIT)
void board_debug_uart_init(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
	init_uart_clk(UART_IDX);
}
#endif

/* called after debug_uart initialization */
int spl_board_boot_device(enum boot_device boot_device_spl)
{
	int ret;

	debug("%s@%d: boot_device_spl=%d ", __func__, __LINE__,
	      boot_device_spl);
	if (IS_ENABLED(CONFIG_SPL_BOOTROM_SUPPORT)) {
		ret = BOOT_DEVICE_BOOTROM; // 15
	} else {
		switch (boot_device_spl) {
		case SD1_BOOT: // 6
		case MMC1_BOOT: // 10
			ret = BOOT_DEVICE_MMC1; // 1
			break;
		case SD2_BOOT: // 7
		case MMC2_BOOT: // 11
			ret = BOOT_DEVICE_MMC2; // 2
			break;
		case SD3_BOOT: // 8
		case MMC3_BOOT: // 12
			ret = BOOT_DEVICE_MMC2; // 3
			break;
		case USB_BOOT: // 17
			ret = BOOT_DEVICE_BOARD; // 12
			break;
		default:
			ret = BOOT_DEVICE_NONE; // 16
		}
	}

	debug("-> %d\n", ret);
	return ret;
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

#ifdef CONFIG_SPL_BOARD_INIT
void spl_board_init(void)
{
	if (IS_ENABLED(CONFIG_SPL_BANNER_PRINT))
		puts("Normal Boot\n");
}
#endif

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	debug("%s: %s\n", __func__, name);
	return 0;
}
#endif

struct mem_region {
	const char *name;
	unsigned long start;
	unsigned long end;
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
};

#ifdef CONFIG_SPL_BSS_MAX_SIZE
#define BSS_SIZE CONFIG_SPL_BSS_MAX_SIZE
#else
#define BSS_SIZE	((uintptr_t)&__bss_end - (uintptr_t)&__bss_start)
#endif

static struct mem_region mem_regions[] = {
	[SPL] = { "SPL", CONFIG_SPL_TEXT_BASE, (uintptr_t)&_end, },
	[DTB] = { "DTB", (uintptr_t)&_end, },
	[DDRFW] = { "DDRFW", },
	[BSS] = { "BSS", (uintptr_t)&__bss_start,
		  (uintptr_t)&__bss_start + BSS_SIZE, },
	[STACK] = { "Stack", },
	[MALLOC] = { "MALLOC", },
	[BL31] = { "BL31", CONFIG_BL31_BASE, BL31_END, },
	[DRAMCFG] = {"DRAMCFG", CONFIG_SAVED_DRAM_TIMING_BASE,
		       CONFIG_SAVED_DRAM_TIMING_BASE + 0x2b00, },
};

static const size_t num_regions = ARRAY_SIZE(mem_regions);

static bool check_region(const struct mem_region *r1, const struct mem_region *r2)
{
	size_t overlap;

	if (r1->start >= r2->end || r1->end <= r2->start)
		return true;

	if (r2->start >= r1->start)
		overlap = min(r1->end - r2->start, r2->end - r2->start);
	else if (r1->start > r2->start)
		overlap = min(r2->end - r1->start, r1->end - r1->start);

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
#define SPL_STACK_END		(CONFIG_SPL_STACK - CONFIG_VAL(SYS_MALLOC_F_LEN))
#define MALLOC_START_ADDR	SPL_SYS_MALLOC_START
#else
#define SPL_STACK_END		CONFIG_SPL_STACK
#define MALLOC_START_ADDR	CFG_MALLOC_F_ADDR
#endif

static void check_mem_regions(struct mem_region *mem_regions, size_t num_regions)
{
	uintptr_t sp;
	uintptr_t eof = (uintptr_t)&_end;
	uintptr_t dtb = (uintptr_t)gd->fdt_blob;
	size_t i, j;
	int err = 0;
	int mri[num_regions];

	memset(mri, 0xff, sizeof(mri));

	asm("mov %0, sp\n" : "=r"(sp));

	mem_regions[STACK].end = SPL_STACK_END;
	mem_regions[STACK].start = SPL_STACK_END - STACK_SIZE - ALIGN(GD_SIZE, 16);

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
	mem_regions[MALLOC].start = MALLOC_START_ADDR;
	mem_regions[MALLOC].end = MALLOC_START_ADDR + CONFIG_VAL(SYS_MALLOC_F_LEN);

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
		}
		if (check_region(r1, r2))
			printf("%-8s:\t%08lx..%08lx (%08lx)\n", r2->name,
			       r2->start, r2->end - 1,
			       r2->end - r2->start);
		else
			err++;
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

	arch_cpu_init();

	spl_wdog_init();

	timer_init();

	ret = spl_init();
	if (ret)
		panic("spl_init() failed: %d\n", ret);

	preloader_console_init();

	check_mem_regions(mem_regions, num_regions);

	enable_tzc380();

	power_init_board();

	/* DDR initialization */
	spl_dram_init();

	board_init_r(NULL, 0);
}
#endif
