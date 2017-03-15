/*
 * Board init file for TXSD-410E
 *
 * (C) Copyright 2017  Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <console.h>
#include <dm.h>
#include <fdt_support.h>
#include <lcd.h>
#include <led.h>
#include <mmc.h>
#include <usb.h>
#include <usb_ether.h>
#include <asm/gpio.h>
#ifdef DEBUG
#include <asm/armv8/mmu.h>
#endif
#include <linux/fb.h>
#include <spmi/spmi.h>
#include <power/pmic.h>
#include <power/pmic-qcom-smd-rpm.h>

#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SHOW_ACTIVITY

enum {
	LED_STATE_INIT,
	LED_STATE_ACTIVE,
	LED_STATE_DISABLED,
};

static int led_state = LED_STATE_INIT;

#define FDT_USER_LED_LABEL	"txsd-410e:green:user1"

void show_activity(int arg)
{
	static struct udevice *led_dev;
	static ulong last;
	int ret;

	if (led_state == LED_STATE_ACTIVE) {
		static int led_on;

		if (get_timer(last) > CONFIG_SYS_HZ) {
			last = get_timer(0);
			led_on = !led_on;
			ret = led_set_on(led_dev, led_on);
			if (ret != 0)
				goto disable;
		}
	} else if (led_state == LED_STATE_DISABLED) {
		return;
	} else if (led_state == LED_STATE_INIT) {
		ret = led_get_by_label(FDT_USER_LED_LABEL, &led_dev);
		if (ret) {
			printf("No '%s' LED found in DTB\n",
				FDT_USER_LED_LABEL);
			goto disable;
		}
		last = get_timer(0);
		led_state = LED_STATE_ACTIVE;
	} else {
		printf("Invalid LED state: %d @ %p\n", led_state, &led_state);
		goto disable;
	}
	return;

disable:
	led_state = LED_STATE_DISABLED;
}
#endif

#ifdef CONFIG_LCD
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 1920,
	.vl_row = 1080,

	.vl_bpix = LCD_COLOR32,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
};

static int lcd_enabled = 1;
static int lcd_bl_polarity;
static struct gpio_desc lcd_bl_gpio;

void lcd_enable(void)
{
	if (lcd_bl_gpio.dev) {
		dm_gpio_set_dir_flags(&lcd_bl_gpio, GPIOD_IS_OUT |
				(lcd_bl_polarity ? GPIOD_ACTIVE_LOW : 0));
	}
}

void lcd_ctrl_init(void *lcdbase)
{
	int color_depth = 24;
	const char *video_mode = karo_get_vmode(getenv("video_mode"));
	const char *vm;
	unsigned long val;
	int refresh = 60;
	struct fb_videomode *p;
	struct fb_videomode fb_mode;
	int xres_set = 0, yres_set = 0, bpp_set = 0, refresh_set = 0;

	if (karo_get_fb_mode(NULL, &p) <= 0)
		goto disable;

	if (!lcd_enabled) {
		debug("LCD disabled\n");
		goto disable;
	}

	if (had_ctrlc() || video_mode == NULL) {
		debug("Disabling LCD\n");
		lcd_enabled = 0;
		if (had_ctrlc())
			setenv("splashimage", NULL);
		goto disable;
	}

	karo_fdt_move_fdt();
	karo_fdt_get_backlight_gpio(gd->fdt_blob, &lcd_bl_gpio);
	lcd_bl_polarity = karo_fdt_get_backlight_polarity(gd->fdt_blob);

	vm = video_mode;
	if (karo_fdt_get_fb_mode(gd->fdt_blob, video_mode, &fb_mode) == 0) {
		p = &fb_mode;
		debug("Using video mode from FDT\n");
		vm += strlen(vm);
		if (fb_mode.xres > panel_info.vl_col ||
			fb_mode.yres > panel_info.vl_row) {
			printf("video resolution from DT: %dx%d exceeds hardware limits: %dx%d\n",
				fb_mode.xres, fb_mode.yres,
				panel_info.vl_col, panel_info.vl_row);
			lcd_enabled = 0;
			goto disable;
		}
	}
	if (p->name != NULL)
		debug("Trying compiled-in video modes\n");
	while (p->name != NULL) {
		if (strcmp(p->name, vm) == 0) {
			debug("Using video mode: '%s'\n", p->name);
			vm += strlen(vm);
			break;
		}
		p++;
	}
	if (*vm != '\0')
		debug("Trying to decode video_mode: '%s'\n", vm);
	while (*vm != '\0') {
		if (*vm >= '0' && *vm <= '9') {
			char *end;

			val = simple_strtoul(vm, &end, 0);
			if (end > vm) {
				if (!xres_set) {
					if (val > panel_info.vl_col)
						val = panel_info.vl_col;
					p->xres = val;
					panel_info.vl_col = val;
					xres_set = 1;
				} else if (!yres_set) {
					if (val > panel_info.vl_row)
						val = panel_info.vl_row;
					p->yres = val;
					panel_info.vl_row = val;
					yres_set = 1;
				} else if (!bpp_set) {
					switch (val) {
					case 24:
					case 18:
						color_depth = val;
						break;

					default:
						printf("Invalid color depth: '%.*s' in video_mode; using default: '%u'\n",
							(int)(end - vm), vm, color_depth);
					}
					bpp_set = 1;
				} else if (!refresh_set) {
					refresh = val;
					refresh_set = 1;
				}
			}
			vm = end;
		}
		switch (*vm) {
		case '@':
			bpp_set = 1;
			/* fallthru */
		case '-':
			yres_set = 1;
			/* fallthru */
		case 'x':
			xres_set = 1;
			/* fallthru */
		case 'M':
		case 'R':
			vm++;
			break;

		default:
			if (*vm != '\0')
				vm++;
		}
	}
	if (p->xres == 0 || p->yres == 0) {
		int num_modes = karo_get_fb_mode(NULL, &p);
		int i;

		printf("Invalid video mode: %s\n", getenv("video_mode"));
		lcd_enabled = 0;
		printf("Supported video modes are:");
		for (i = 0; i < num_modes; i++, p++) {
			if (p->name)
				printf(" %s", p->name);
		}
		printf("\n");
		goto disable;
	}
	if (p->xres > panel_info.vl_col || p->yres > panel_info.vl_row) {
		printf("video resolution: %dx%d exceeds hardware limits: %dx%d\n",
			p->xres, p->yres, panel_info.vl_col, panel_info.vl_row);
		lcd_enabled = 0;
		goto disable;
	}
	panel_info.vl_col = p->xres;
	panel_info.vl_row = p->yres;

	switch (color_depth) {
	case 8:
		panel_info.vl_bpix = LCD_COLOR8;
		break;
	case 16:
		panel_info.vl_bpix = LCD_COLOR16;
		break;
	default:
		panel_info.vl_bpix = LCD_COLOR32;
	}

	p->pixclock = KHZ2PICOS(refresh *
		(p->xres + p->left_margin + p->right_margin + p->hsync_len) *
		(p->yres + p->upper_margin + p->lower_margin + p->vsync_len) /
				1000);
	debug("Pixel clock set to %lu.%03lu MHz\n",
		PICOS2KHZ(p->pixclock) / 1000, PICOS2KHZ(p->pixclock) % 1000);

	if (p != &fb_mode) {
		int ret;

		debug("Creating new display-timing node from '%s'\n",
			video_mode);
		ret = karo_fdt_create_fb_mode(working_fdt, video_mode, p);
		if (ret)
			printf("Failed to create new display-timing node from '%s': %d\n",
				video_mode, ret);
	}

#if 0
	gpio_request_array(stk5_lcd_gpios, ARRAY_SIZE(stk5_lcd_gpios));
	imx_iomux_v3_setup_multiple_pads(stk5_lcd_pads,
					ARRAY_SIZE(stk5_lcd_pads));
#endif
	if (karo_load_splashimage(0) == 0) {
		debug("Initializing LCD controller\n");
	} else {
		debug("Skipping initialization of LCD controller\n");
	}
	return;

disable:
	lcd_enabled = 0;
	panel_info.vl_col = 0;
	panel_info.vl_row = 0;
}
#endif /* CONFIG_LCD */

void txsd_mmc_preinit(void)
{
	struct udevice *mmcdev;

	for (uclass_first_device(UCLASS_MMC, &mmcdev); mmcdev;
	     uclass_next_device(&mmcdev)) {
		struct mmc *m = mmc_get_mmc_dev(mmcdev);
		if (m)
			mmc_set_preinit(m, 1);
	}
}

int dram_init(void)
{
#ifdef DEBUG
	const struct mm_region *mm = mem_map;
	int i;

	for (i = 0; mm->size; i++, mm++) {
		printf("MMU region[%d]=%08llx..%08llx -> %08llx..%08llx size: %08llx Attrs: %08llx\n",
			i, mm->phys, mm->phys + mm->size - 1,
			mm->virt, mm->virt + mm->size - 1, mm->size, mm->attrs);
	}
#endif
#if CONFIG_NR_DRAM_BANKS == 1
	gd->ram_size = PHYS_SDRAM_1_SIZE;
#else
	gd->ram_size = PHYS_SDRAM_1_SIZE + PHYS_SDRAM_2_SIZE;
#endif
	return 0;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
	printf("RAM bank 0 size set to %lluMiB\n", gd->bd->bi_dram[0].size / SZ_1M);
#if CONFIG_NR_DRAM_BANKS > 1
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
	printf("RAM bank 1 size set to %lluMiB\n", gd->bd->bi_dram[1].size / SZ_1M);
#endif
	printf("Total RAM size: %lluMiB\n", gd->ram_size / SZ_1M);
}

/* run with default environment */
int board_init(void)
{
	if (ctrlc()) {
		printf("<CTRL-C> detected; safeboot enabled\n");
		return 0;
	}
	txsd_mmc_preinit();
	return 0;
}

/* run with env from mass storage device */
static inline int fdt_err_to_errno(int fdterr)
{
	switch (-fdterr) {
	case FDT_ERR_NOTFOUND:
		return -ENOENT;
	case FDT_ERR_EXISTS:
		return -EEXIST;
	case FDT_ERR_NOSPACE:
		return -ENOMEM;
	case FDT_ERR_BADOFFSET:
	case FDT_ERR_BADPATH:
	case FDT_ERR_BADPHANDLE:
	case FDT_ERR_BADSTATE:
		return -EINVAL;
	case FDT_ERR_TRUNCATED:
		return -EILSEQ;
	case FDT_ERR_BADMAGIC:
	case FDT_ERR_BADVERSION:
	case FDT_ERR_BADSTRUCTURE:
	case FDT_ERR_BADLAYOUT:
	case FDT_ERR_INTERNAL:
	case FDT_ERR_BADNCELLS:
	case FDT_ERR_TOODEEP:
		return -EINVAL;
	}
	return -EBADFD;
}

int board_prepare_usb(enum usb_init_type type)
{
	static struct gpio_desc vbusen;
	static int inited;
	int ret;

	printf("Setting up USB port as %s\n",
		type == USB_INIT_HOST ? "host" :
		USB_INIT_DEVICE ? "device" : "invalid");
	if (type != USB_INIT_HOST && type != USB_INIT_DEVICE)
		printf("Invalid USB type: %08x\n", type);

	if (!inited) {
		int off = fdt_path_offset(gd->fdt_blob, "usbhost");

		if (off < 0) {
			printf("Could not find 'usbost' node: %s\n",
				fdt_strerror(off));
			return -ENODEV;
		}
		ret = gpio_request_by_name_nodev(gd->fdt_blob, off,
						"switch-gpio", 0, &vbusen, 0);
		if (ret) {
			printf("Failed to request VBUSEN GPIO: %d\n", ret);
			return ret;
		}
		inited = 1;
	}

	ret = dm_gpio_set_value(&vbusen, type == USB_INIT_HOST);
	if (ret == 0)
		ret = dm_gpio_set_dir_flags(&vbusen, GPIOD_IS_OUT);
	if (ret == 0)
		ret = dm_gpio_set_value(&vbusen, type == USB_INIT_HOST);

	return ret;
}

static int karo_spmi_init(void)
{
	int ret;
	struct udevice *dev;

	ret = uclass_get_device_by_name(UCLASS_SPMI, "spmi", &dev);
	if (ret)
		return ret;

	/* Configure PON_PS_HOLD_RESET_CTL for HARD reset */
	ret = spmi_reg_write(dev, 0, 8, 0x5a, 7);
	return ret;
}

/* Check for <CTRL-C> - if pressed - stop autoboot */
int misc_init_r(void)
{
	unsigned long fdt_addr __maybe_unused = getenv_ulong("fdtaddr", 16, 0);
	int ret;

	env_cleanup();

	if (had_ctrlc()) {
		setenv_ulong("safeboot", 1);
		return 0;
	}
		setenv("safeboot", NULL);

	ret = karo_spmi_init();
	if (ret)
		printf("Failed to initialize SPMI interface: %d\n", ret);

	karo_fdt_move_fdt();

	if (getenv("usbethaddr") && !had_ctrlc()) {
		uchar mac_addr[ETH_ALEN];

		ret = usb_init();
		if (ret < 0) {
			printf("USB init failed: %d\n", ret);
			return 0;
		}
		if (eth_getenv_enetaddr("usbethaddr", mac_addr))
			printf("MAC address: %pM\n", mac_addr);

	} else {
		printf("'usbethaddr' not set; skipping USB initialization\n");
	}
	return 0;
}

#ifdef CONFIG_QCOM_SMD_RPM
#define LDO(n, uV, en)	uint32_t ldo##n[] = {			\
		LDOA_RES_TYPE, n, KEY_SOFTWARE_ENABLE, 4, GENERIC_##en, \
		KEY_MICRO_VOLT, 4, uV, }

static LDO(2, 1200000, ENABLE);		// LPDDR
static LDO(3, 1150000, ENABLE);		// VDD_MEM, PLL, USB
static LDO(5, 1800000, ENABLE);		// LPDDR I/O
static LDO(7, 1800000, ENABLE);		// WLAN OSC, PLL2, VDDA2, USBPHY
static LDO(8, 2900000, ENABLE);		// eMMC
static LDO(15, 1800000, ENABLE);	// Basisboard VDDIO + L16
static LDO(16, 1800000, ENABLE);	// Basisboard VDDIO + L15 (55mA)
static LDO(17, 3300000, ENABLE);	// Basisboard VDD33 (450mA)

static LDO(1, 1225000, DISABLE);	// ext. Conn.
static LDO(6, 1800000, DISABLE);	// MIPI + Temp Sens.
static LDO(9, 3300000, DISABLE);	// WLAN
static LDO(11, 1900000, ENABLE);	// JTAG, ext. Conn. OWIRE
static LDO(12, 2900000, ENABLE);	// SD-Card
static LDO(13, 3075000, ENABLE);	// USBPHY

static LDO(4, 1800000, DISABLE);	// NC
static LDO(10, 1000000, DISABLE);	// NC
static LDO(14, 1000000, DISABLE);	// NC
static LDO(18, 1000000, DISABLE);	// NC

#define _debug(fmt...) do {} while (0)

static inline void __smd_regulator_control(uint32_t *data, size_t len,
				const char *name)
{
	int ret;

	_debug("%s@%d: %sabling %s: %u.%03uV\n", __func__, __LINE__,
		data[4] == GENERIC_ENABLE ? "En" : "Dis", name,
		data[7] / 1000000, data[7] / 1000 % 1000);
	ret = rpm_send_data(data, len, RPM_REQUEST_TYPE);
	if (ret)
		printf("Failed to configure regulator %s\n", name);
	udelay(10000);
}

#define smd_regulator_control(n)	__smd_regulator_control(n, sizeof(n), #n)

static void smd_pmic_setup(void)
{
	smd_rpm_init();

	smd_regulator_control(ldo1);
	smd_regulator_control(ldo2);
	smd_regulator_control(ldo3);
	smd_regulator_control(ldo4);
	smd_regulator_control(ldo5);
	smd_regulator_control(ldo6);
	smd_regulator_control(ldo7);
	smd_regulator_control(ldo8);
	smd_regulator_control(ldo9);
	smd_regulator_control(ldo10);
	smd_regulator_control(ldo11);
	smd_regulator_control(ldo12);
	smd_regulator_control(ldo13);
	smd_regulator_control(ldo14);
	smd_regulator_control(ldo15);
	smd_regulator_control(ldo16);
	smd_regulator_control(ldo17);
	smd_regulator_control(ldo18);

	/* turn off unused regulators */
	smd_regulator_control(ldo4);
	smd_regulator_control(ldo10);
	smd_regulator_control(ldo14);
	smd_regulator_control(ldo18);

	/* enable all essential regulators */
	smd_regulator_control(ldo2);
	smd_regulator_control(ldo3);
	smd_regulator_control(ldo5);
	smd_regulator_control(ldo7);
	smd_regulator_control(ldo8);
	smd_regulator_control(ldo15);
	smd_regulator_control(ldo16);
	smd_regulator_control(ldo17);

	/* setup optional regulators */
	smd_regulator_control(ldo1);
	smd_regulator_control(ldo6);
	smd_regulator_control(ldo9);
	smd_regulator_control(ldo11);
	smd_regulator_control(ldo12);
	smd_regulator_control(ldo13);

	smd_rpm_uninit();
}
#else
static inline void smd_pmic_setup(void)
{
}
#endif

int board_late_init(void)
{
	if (!getenv("safeboot") && !ctrlc())
		smd_pmic_setup();

	clear_ctrlc();
	return 0;
}

static const char *txsd_touchpanels[] = {
	"edt,edt-ft5x06",
	"eeti,egalax_ts",
};

int ft_board_setup(void *blob, bd_t *bd)
{
	int ret;
	const char *video_mode = karo_get_vmode(getenv("video_mode"));

	ret = fdt_increase_size(blob, 4096);
	if (ret) {
		printf("Failed to increase FDT size: %s\n", fdt_strerror(ret));
		return ret;
	}
	karo_fdt_fixup_touchpanel(blob, txsd_touchpanels,
				ARRAY_SIZE(txsd_touchpanels));
	karo_fdt_update_fb_mode(blob, video_mode);
	smd_rpm_uninit();
	return 0;
}

static int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	struct udevice *dev;

	ret = uclass_get_device_by_name(UCLASS_SPMI, "spmi", &dev);
	if (ret) {
		printf("Failed to get SPMI bus: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	ret = spmi_reg_write(dev, 0, 8, 0x8f, 8);
	if (ret)
		return CMD_RET_FAILURE;

	do_reset(NULL, 0, 0, NULL);
	return CMD_RET_FAILURE;
}

U_BOOT_CMD(
	fastboot, 2, 1, do_fastboot,
	"reboot into Fastboot protocol\n",
	"    - run as a fastboot usb device"
);
