// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <errno.h>
#include <dwc3-uboot.h>
#include <fdtdec.h>
#include <fsl_esdhc_imx.h>
#include <hang.h>
#include <hexdump.h>
#include <i2c.h>
#include <init.h>
#include <malloc.h>
#include <mmc.h>
#include <spl.h>
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
		(image_entry_noargs_t)spl_image->entry_point;
#ifdef DEBUG
	uintptr_t sp;

	asm("\tmov	%0, sp\n" : "=r"(sp));
	debug("image entry point: 0x%lx sp=%08lx\n",
	      spl_image->entry_point, sp);
	debug("fdtaddr=%p\n", fdt_addr);
#if CONFIG_IS_ENABLED(SYS_MALLOC_SIMPLE)
	malloc_simple_info();
	printf("@%08lx..%08lx\n", gd->malloc_base,
	       gd->malloc_base + gd->malloc_ptr - 1);
#endif /* SPL_SYS_MALLOC_SIMPLE */
#endif /* DEBUG */

	asm("\tmov x1, %0\n"
	    :: "r"(fdt_addr) : "x0", "x1", "x2", "x3");
	image_entry();
}

#if CONFIG_IS_ENABLED(SERIAL) || defined(CONFIG_DEBUG_UART)
#define UART_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_FSEL1 |	\
					     PAD_CTL_DSE6)
/*
 * changing the UART setup here requires changes to the ATF too!
 */
#if defined(CONFIG_IMX8MM)
#define UART_IDX 0
static const iomux_v3_cfg_t uart_pads[] = {
	IMX8MM_PAD_UART1_RXD_UART1_RX | UART_PAD_CTRL,
	IMX8MM_PAD_UART1_TXD_UART1_TX | UART_PAD_CTRL,
};
#elif defined(CONFIG_IMX8MN)
#define UART_IDX 0
static const iomux_v3_cfg_t uart_pads[] = {
	IMX8MN_PAD_UART1_RXD__UART1_DCE_RX | UART_PAD_CTRL,
	IMX8MN_PAD_UART1_TXD__UART1_DCE_TX | UART_PAD_CTRL,
};
#elif defined(CONFIG_IMX8MP)
#if CONFIG_MXC_UART_BASE == UART1_BASE_ADDR
#define UART_IDX 0
static const iomux_v3_cfg_t uart_pads[] = {
	MX8MP_PAD_UART1_RXD__UART1_DCE_RX | UART_PAD_CTRL,
	MX8MP_PAD_UART1_TXD__UART1_DCE_TX | UART_PAD_CTRL,
};
#elif CONFIG_MXC_UART_BASE == UART2_BASE_ADDR
#define UART_IDX 1
static const iomux_v3_cfg_t uart_pads[] = {
	MX8MP_PAD_UART2_RXD__UART2_DCE_RX | UART_PAD_CTRL,
	MX8MP_PAD_UART2_TXD__UART2_DCE_TX | UART_PAD_CTRL,
};
#else
#error unsupported UART selected with CONFIG_MXC_UART_BASE
#endif /* CONFIG_MXC_UART_BASE == */
#else
#error Unsupported SoC
#endif /* CONFIG_IMX8MM */
#endif /* CONFIG_SERIAL || CONFIG_DEBUG_UART */

/* called before debug_uart is initialized */
#ifdef CONFIG_IMX8MP
#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const wdog_pads[] = {
	MX8MP_PAD_GPIO1_IO02__WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
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

#if CONFIG_IS_ENABLED(SERIAL) || IS_ENABLED(CONFIG_DEBUG_UART)
static void spl_uart_init(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
	init_uart_clk(UART_IDX);
	if (IS_ENABLED(CONFIG_DEBUG_UART)) {
		debug_uart_init();
		printascii("enabled\n");
	}
}
#else
static inline void spl_uart_init(void)
{
}
#endif

#if IS_ENABLED(CONFIG_USB_DWC3_GADGET) && !CONFIG_IS_ENABLED(DM_USB_GADGET)
int usb_gadget_handle_interrupts(void)
{
	dwc3_uboot_handle_interrupt(0);
	return 0;
}
#endif

int board_early_init_f(void)
{
	spl_wdog_init();
	spl_uart_init();

	return 0;
}

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
	spl_uart_init();
}
#endif

/* called after debug_uart initialization */
int spl_board_boot_device(enum boot_device boot_device_spl)
{
	enum boot_device ret;

	debug("%s@%d: boot_device_spl=%d ", __func__, __LINE__,
	      boot_device_spl);
#ifdef CONFIG_SPL_BOOTROM_SUPPORT
	ret = BOOT_DEVICE_BOOTROM; // 15
#else
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
#endif
	debug("-> %d\n", ret);
	return ret;
}

void spl_dram_init(void)
{
	unsigned long sp;

	asm("mov %0, sp\n" : "=r"(sp));

	debug("%s@%d: SP=%08lx\n", __func__, __LINE__, sp);
	ddr_init(&dram_timing);
	debug("%s@%d: dram_init done\n", __func__, __LINE__);
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

#ifndef CONFIG_SPL_FRAMEWORK_BOARD_INIT_F
#ifdef DEBUG
#if defined(CONFIG_IMX8MM)
#define BL31_START	0x920000UL
#define BL31_END	0x940000UL
#elif defined(CONFIG_IMX8MN)
#define BL31_START	0x960000UL
#define BL31_END	0x980000UL
#elif defined(CONFIG_IMX8MP)
#define BL31_START	0x970000UL
#define BL31_END	0x990000UL
#endif

#if IS_ENABLED(CONFIG_IMX8M_DDR3L)
#define DDRFW_SIZE	(SZ_32K + SZ_16K)
#else
#define DDRFW_SIZE	(2 * (SZ_32K + SZ_16K))
#endif

enum mem_regions {
	SPL,
	DTB,
	DDRFW,
	BSS,
	STACK,
	MALLOC,
	BL31,
};

static struct mem_region {
	const char *name;
	unsigned long start;
	unsigned long end;
} mem_regions[] = {
	[SPL] = { "SPL", CONFIG_SPL_TEXT_BASE, (unsigned long)&_end, },
	[DTB] = { "DTB", (unsigned long)&_end, },
	[DDRFW] = { "DDRFW", },
	[BSS] = { "BSS", (unsigned long)&__bss_start, (unsigned long)&__bss_end, },
	[STACK] = { "Stack", },
	[MALLOC] = { "MALLOC", },
	[BL31] = { "BL31", BL31_START, BL31_END, },
};

static int check_region(const struct mem_region *r1, const struct mem_region *r2)
{
	if (r1->start >= r2->end || r1->end <= r2->start)
		return 0;
	printf("%s %08lx..%08lx overlaps %s %08lx..%08lx\n", r1->name,
	       r1->start, r1->end - 1, r2->name, r2->start, r2->end - 1);
	return 1;
}

#ifdef CONFIG_SPL_SIZE_LIMIT_PROVIDE_STACK
#define STACK_SIZE	CONFIG_VAL(SIZE_LIMIT_PROVIDE_STACK)
#else
#define STACK_SIZE	SZ_8K
#endif

void check_mem_regions(void)
{
	unsigned long sp;
	unsigned long eof = (unsigned long)&_end;
	unsigned long dtb = (unsigned long)gd->fdt_blob;
	size_t i, j;
	int err = 0;
	int mri[ARRAY_SIZE(mem_regions)] = { -1, };

	asm("mov %0, sp\n" : "=r"(sp));

	mem_regions[STACK].end = CONFIG_SPL_STACK - CONFIG_VAL(SYS_MALLOC_F_LEN);
	mem_regions[STACK].start = mem_regions[STACK].end - STACK_SIZE;

	if (sp < mem_regions[STACK].start)
		panic("Stack overflow: sp=%08lx [%08lx..%08lx]\n", sp,
		      mem_regions[STACK].start, mem_regions[STACK].end);
	if (sp >= mem_regions[STACK].end)
		panic("Stack underflow: sp=%08lx [%08lx..%08lx]\n", sp,
		      mem_regions[STACK].start, mem_regions[STACK].end);

	if (gd->fdt_blob && !fdt_check_header(gd->fdt_blob)) {
		eof += fdt_totalsize(gd->fdt_blob);
		mem_regions[DTB].start = dtb;
		mem_regions[DTB].end = dtb + fdt_totalsize(gd->fdt_blob);
	} else {
		printf("No valid DTB found\n");
	}
	mem_regions[DDRFW].start = eof;
	mem_regions[DDRFW].end = eof + DDRFW_SIZE;
	mem_regions[MALLOC].start = gd->malloc_base;
	mem_regions[MALLOC].end = gd->malloc_base + gd->malloc_limit;

	for (i = j = 0; i < ARRAY_SIZE(mem_regions); i++) {
		int k;

		if (!mem_regions[i].start || !mem_regions[i].end)
			continue;
		mri[j++] = i;
		if (i == 0)
			continue;
		for (k = j; k > 0; k--) {
			if (mem_regions[mri[k - 1]].start < mem_regions[i].start) {
				break;
			}
			mri[k] = mri[k - 1];
			mri[k - 1] = i;
		}
	}
	for (i = 0; i < ARRAY_SIZE(mri); i++) {
		if (mri[i] < 0)
			break;
		j = mri[i];
		printf("%s:\t\t%08lx..%08lx\n", mem_regions[j].name,
		       mem_regions[j].start, mem_regions[j].end);
	}
	for (i = 0; i < ARRAY_SIZE(mem_regions); i++) {
		if (!mem_regions[i].start || !mem_regions[i].end)
			continue;
		for (j = i + 1; j < ARRAY_SIZE(mem_regions); j++) {
			if (!mem_regions[j].start || !mem_regions[j].end)
				continue;
			err |= check_region(&mem_regions[i], &mem_regions[j]);
		}
	}
	if (err)
		panic("Memory regions overlap detected\n");
}

/*
SPL:            00912000..0092a120
DTB:            0092a120..0092be08
DDRFW:          0092be08..00937e08
BSS:            0094f800..0094f870
BSS:            0094f800..0094f870
BSS:            0094f800..0094f870
BL31:           00960000..00980000
*/
#else
static inline void check_mem_regions(void)
{
}
#endif

void board_init_f(ulong dummy)
{
	int ret;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	board_early_init_f();

	timer_init();

	ret = spl_init();
	if (ret)
		panic("spl_init() failed: %d\n", ret);

	preloader_console_init();

	check_mem_regions();

	enable_tzc380();

	power_init_board();

	/* DDR initialization */
	spl_dram_init();

	board_init_r(NULL, 0);
}
#endif
