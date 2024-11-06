// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Lothar Waßmann <LW@KARO-electronics.de>
 *   based on: board/st/stm32mp2/stm32mp2.c
 *   Copyright (C) 2022 STMicroelectronics - All Rights Reserved
 */

#include <common.h>
#include <config.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <fdt_support.h>
#include <fuse.h>
#include <g_dnl.h>
#include <i2c.h>
#include <led.h>
#include <log.h>
#include <misc.h>
#include <mmc.h>
#include <mtd_node.h>
#include <init.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <regmap.h>
#include <rng.h>
#include <syscon.h>
#include <system-constants.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/sections.h>
#include <asm/arch/sys_proto.h>
#include <dm/device.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>
#include <dm/ofnode.h>
#include <dm/uclass.h>
#include <dt-bindings/gpio/gpio.h>
#include <jffs2/load_kernel.h>
#include <linux/delay.h>

#ifdef CONFIG_VIDEO_LOGO
#include <video.h>
#include <bmp_logo.h>
#endif

#include "../common/karo.h"

#define SYSCFG_ETHCR_ETH_SEL_MII	0
#define SYSCFG_ETHCR_ETH_SEL_RGMII	BIT(4)
#define SYSCFG_ETHCR_ETH_SEL_RMII	BIT(6)
#define SYSCFG_ETHCR_ETH_CLK_SEL	BIT(1)
#define SYSCFG_ETHCR_ETH_REF_CLK_SEL	BIT(0)

/* CLOCK feed to PHY */
#define ETH_CK_F_25M	25000000
#define ETH_CK_F_50M	50000000
#define ETH_CK_F_125M	125000000

/*
 * Get a global data pointer
 */
DECLARE_GLOBAL_DATA_PTR;

#if CONFIG_IS_ENABLED(VIDEO_LOGO)
static void show_bmp_logo(void)
{
	int ret;
	struct udevice *dev;

	ret = uclass_get_device(UCLASS_VIDEO, 0, &dev);
	debug("uclass_get_device(UCLASS_VIDEO) returned %d\n", ret);
	if (ret)
		return;

	video_bmp_display(dev, (uintptr_t)bmp_logo_bitmap,
			  0, 0, false);
}
#else
static inline void show_bmp_logo(void)
{
}
#endif

#if IS_ENABLED(CONFIG_KARO_UBOOT_MFG) || defined(DEBUG)
enum mem_regions {
	TEXT,
	DTB,
	BSS,
	STACK,
	MALLOC,
	TFA,
	OPTEE,
};

#define TFA_START	0x0e000000
#define TFA_END		0x0e040000

#define OPTEE_START	0x82000000
#define OPTEE_END	0x83000000

static struct mem_region {
	const char *name;
	unsigned long start;
	unsigned long end;
} mem_regions[] = {
	[TEXT] = { "U-Boot", (unsigned long)&__image_copy_start,
		(unsigned long)&__image_copy_end, },
	[DTB] = { "DTB", (unsigned long)&_end, },
	[BSS] = { "BSS", (unsigned long)&__bss_start,
		(unsigned long)&__bss_end, },
	[STACK] = { "STACK", },
	[MALLOC] = { "MALLOC", },
	[TFA] = { "TF-A", TFA_START, TFA_END, },
	[OPTEE] = { "OPTEE", OPTEE_START, OPTEE_END, },
};

static int check_region(const struct mem_region *r1, const struct mem_region *r2)
{
	if (r1->start >= r2->end || r1->end <= r2->start)
		return 0;
	printf("%s:\t%08lx..%08lx overlaps %s %08lx..%08lx\n", r1->name,
	       r1->start, r1->end - 1, r2->name, r2->start, r2->end - 1);
	return 1;
}

#define STACK_SIZE	SZ_8K

void check_mem_regions(void)
{
	unsigned long sp;
	unsigned long eof = (unsigned long)&_end;
	unsigned long dtb = (unsigned long)gd->fdt_blob;
	size_t i, j;
	int err = 0;
	int mri[ARRAY_SIZE(mem_regions)] = { -1, };

	asm("mov %0, sp\n" : "=r"(sp));

	mem_regions[STACK].end = SYS_INIT_SP_ADDR - CONFIG_VAL(SYS_MALLOC_F_LEN);
	mem_regions[STACK].start = mem_regions[STACK].end - STACK_SIZE;

	if (sp < mem_regions[STACK].start)
		printf("Stack overflow: sp=%08lx [%08lx..%08lx]\n", sp,
		       mem_regions[STACK].start, mem_regions[STACK].end - 1);
	if (sp >= mem_regions[STACK].end)
		printf("Stack underflow: sp=%08lx [%08lx..%08lx]\n", sp,
		       mem_regions[STACK].start, mem_regions[STACK].end - 1);

	if (gd->fdt_blob && !fdt_check_header(gd->fdt_blob)) {
		eof += fdt_totalsize(gd->fdt_blob);
		mem_regions[DTB].start = dtb;
		mem_regions[DTB].end = dtb + fdt_totalsize(gd->fdt_blob);
	} else {
		printf("No valid DTB found\n");
	}
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
			if (mem_regions[mri[k - 1]].start < mem_regions[i].start)
				break;

			mri[k] = mri[k - 1];
			mri[k - 1] = i;
		}
	}
	for (i = 0; i < ARRAY_SIZE(mri); i++) {
		if (mri[i] < 0)
			break;
		j = mri[i];
		printf("%s:\t\t%08lx..%08lx\n", mem_regions[j].name,
		       mem_regions[j].start, mem_regions[j].end - 1);
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
#ifdef DEBUG
		printf("Memory regions overlap detected\n");
#else
		panic("Memory regions overlap detected\n");
#endif
}
#else
static inline void check_mem_regions(void)
{
}
#endif

int checkboard(void)
{
	const char *fdt_compat;
	int fdt_compat_len;

	debug("%s@%d:\n", __func__, __LINE__);

	check_mem_regions();

	printf("Board: TXMP-2550");

	fdt_compat = ofnode_get_property(ofnode_root(), "compatible", &fdt_compat_len);
	if (fdt_compat && fdt_compat_len)
		printf(" (%s)", fdt_compat);
	puts("\n");

	return 0;
}

#ifdef CONFIG_USB_GADGET_DOWNLOAD
#define STM32MP1_G_DNL_DFU_PRODUCT_NUM 0xdf11
#define STM32MP1_G_DNL_FASTBOOT_PRODUCT_NUM 0x0afb

int g_dnl_bind_fixup(struct usb_device_descriptor *dev, const char *name)
{
	if (IS_ENABLED(CONFIG_DFU_OVER_USB) &&
	    !strcmp(name, "usb_dnl_dfu"))
		put_unaligned(STM32MP1_G_DNL_DFU_PRODUCT_NUM, &dev->idProduct);
	else if (IS_ENABLED(CONFIG_FASTBOOT) &&
		 !strcmp(name, "usb_dnl_fastboot"))
		put_unaligned(STM32MP1_G_DNL_FASTBOOT_PRODUCT_NUM,
			      &dev->idProduct);
	else
		put_unaligned(CONFIG_USB_GADGET_PRODUCT_NUM, &dev->idProduct);

	return 0;
}
#endif /* CONFIG_USB_GADGET_DOWNLOAD */

#if CONFIG_IS_ENABLED(LED)
enum {
	LED_STATE_INIT = -1,
	LED_STATE_OFF,
	LED_STATE_ON,
	LED_STATE_DISABLED,
};

static int led_state = LED_STATE_INIT;
static struct udevice *led_dev;

static int txmp_get_led(struct udevice **dev, char *led_string)
{
	const char *led_name;
	int ret;

	if (led_state == LED_STATE_DISABLED)
		return -ENODEV;

	led_name = ofnode_conf_read_str(led_string);
	if (!led_name) {
		debug("%s(): could not find %s config string\n",
		      __func__, led_string);
		return -ENOENT;
	}
	ret = led_get_by_label(led_name, dev);
	if (ret)
		pr_err("led_get_by_label() returned: %d\n", ret);

	return ret;
}

static void txmp_setup_led(void)
{
	int ret;

	ret = txmp_get_led(&led_dev, "u-boot,boot-led");
	if (ret) {
		debug("No boot-led defined\n");
		led_state = LED_STATE_DISABLED;
	}
}

#ifdef CONFIG_SHOW_ACTIVITY
void show_activity(int arg)
{
	static int blink_rate = CONFIG_SYS_HZ / 2;
	static ulong last;
	int ret;

	if (!led_dev || led_state == LED_STATE_DISABLED)
		return;

	if (led_state == LED_STATE_INIT) {
		last = get_timer(0);
		ret = led_set_state(led_dev, LEDST_ON);
		if (ret) {
			led_state = LED_STATE_DISABLED;
			return;
		}
		led_state = LED_STATE_ON;
	}
	if (get_timer(last) > blink_rate) {
		last = get_timer(0);
		if (led_state == LED_STATE_ON) {
			led_set_state(led_dev, LEDST_OFF);
			led_state = LED_STATE_OFF;
		} else {
			led_set_state(led_dev, LEDST_ON);
			led_state = LED_STATE_ON;
		}
	}
}
#endif /* CONFIG_SHOW_ACTIVITY */
#endif /* CONFIG_LED */

/* board dependent setup after reloc */
int board_init(void)
{
	struct udevice *dev;

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	/* probe all PINCTRL for hog */
	for (uclass_first_device(UCLASS_PINCTRL, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		debug("probe pincontrol = %s\n", dev->name);
	}

	txmp_setup_led();

	return 0;
}

int get_eth_nb(void)
{
	int eth_ports = 2;
	const char *baseboard = env_get("baseboard");

	if (baseboard) {
		if (strcmp(baseboard, "mb7") == 0)
			eth_ports = 1;
		else if (strcmp(baseboard, "lvds-mb") == 0)
			eth_ports = 1;
		debug("%s@%d: %u ethernet port(s) for %s board\n",
		      __func__, __LINE__, eth_ports, baseboard);
	} else {
		debug("%s@%d: %u ethernet port(s)\n", __func__, __LINE__, eth_ports);
	}
	return eth_ports;
}

static void print_mac_from_fuse(void)
{
	u32 fuse[2];
	u8 mac[ETH_ALEN];

	fuse_read(0, BSEC_OTP_MAC, &fuse[0]);
	fuse_read(0, BSEC_OTP_MAC + 1, &fuse[1]);
	memcpy(mac, fuse, ETH_ALEN);

	if (is_valid_ethaddr(mac))
		printf("MAC addr from fuse: %pM\n", mac);
}

/* eth init function : weak called in eqos driver */
int board_interface_eth_init(struct udevice *dev,
			     phy_interface_t interface_type, ulong rate)
{
	struct regmap *regmap;
	uint regmap_mask, regmap_offset;
	int ret;
	u32 value;
	bool ext_phyclk;

	/* Ethernet PHY has no crystal or needs to be clocked by RCC */
	ext_phyclk = dev_read_bool(dev, "st,ext-phyclk");

	regmap = syscon_regmap_lookup_by_phandle(dev, "st,syscon");
	if (!IS_ERR(regmap)) {
		u32 fmp[3];

		ret = dev_read_u32_array(dev, "st,syscon", fmp, ARRAY_SIZE(fmp));
		if (ret) {
			dev_err(dev, "failed to read st,syscon property: %d\n",
				ret);
			return ret;
		}
		regmap_mask = fmp[2];
		regmap_offset = fmp[1];
	} else {
		dev_err(dev, "st,syscon property is missing in DTB\n");
		return -ENODEV;
	}

	switch (interface_type) {
	case PHY_INTERFACE_MODE_MII:
		value = SYSCFG_ETHCR_ETH_SEL_MII;
		debug("%s: PHY_INTERFACE_MODE_MII\n", __func__);
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (rate == ETH_CK_F_50M && ext_phyclk)
			value = SYSCFG_ETHCR_ETH_SEL_RMII |
				SYSCFG_ETHCR_ETH_REF_CLK_SEL;
		else
			value = SYSCFG_ETHCR_ETH_SEL_RMII;
		debug("%s: PHY_INTERFACE_MODE_RMII\n", __func__);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		if (rate == ETH_CK_F_125M && ext_phyclk)
			value = SYSCFG_ETHCR_ETH_SEL_RGMII |
				SYSCFG_ETHCR_ETH_CLK_SEL;
		else
			value = SYSCFG_ETHCR_ETH_SEL_RGMII;
		debug("%s: PHY_INTERFACE_MODE_RGMII\n", __func__);
		break;
	default:
		debug("%s: Do not manage %d interface\n",
		      __func__, interface_type);
		/* Do not manage other interfaces */
		return -EINVAL;
	}

	ret = regmap_update_bits(regmap, regmap_offset, regmap_mask, value);

	return ret;
}

static inline void txmp_set_bootdevice(void)
{
	const char *bootdev = env_get("boot_device");

	debug("%s@%d:\n", __func__, __LINE__);

	if (!bootdev) {
		printf("boot_device is not set\n");
		return;
	}
	debug("%s@%d: boot_device='%s' boot_instance='%s' preboot='%s'\n",
	      __func__, __LINE__, bootdev,
	      env_get("boot_instance"), env_get("preboot"));
	if (strcmp(bootdev, "mmc") == 0) {
		unsigned long instance = env_get_ulong("boot_instance", 0, 0);

		if (instance == 1) {
			instance = 0;
			env_set_ulong("boot_instance", instance);
		}
	} else if (strcmp(bootdev, "usb") == 0) {
		const char *bootcmd = env_get("bootcmd");
		if (CONFIG_IS_ENABLED(CMD_STM32PROG)) {
			/* Save original 'bootcmd' for restoration after stm32prog */
			env_set(".bootcmd", bootcmd);
			env_set("bootcmd", "stm32prog ${boot_device} ${boot_instance}");
		}
	}
}

static inline void rand_init(void)
{
	unsigned int seed;
	int ret;
	struct udevice *dev;

	ret = uclass_get_device(UCLASS_RNG, 0, &dev);
	if (ret) {
		printf("Failed to get RNG device: %d\n", ret);
		return;
	}

	ret = dm_rng_read(dev, &seed, sizeof(seed));
	if (ret)
		printf("Failed to read RNG: %d\n", ret);

	debug("RANDOM seed: %08x\n", seed);
	srand(seed);
}

int board_late_init(void)
{
	if (CONFIG_IS_ENABLED(ENV_VARS_UBOOT_RUNTIME_CONFIG)) {
		const void *fdt_compat;
		ofnode root = ofnode_path("/");

		if (ofnode_valid(root))
			fdt_compat = ofnode_read_string(root, "compatible");

		if (fdt_compat) {
			if (strncmp(fdt_compat, "karo,", 5) != 0)
				env_set("board_name", fdt_compat);
			else
				env_set("board_name", fdt_compat + 5);
		}
	}

	print_mac_from_fuse();

	karo_env_cleanup();

	if (ctrlc()) {
		printf("<CTRL-C> detected; safeboot enabled\n");
		env_set_hex("safeboot", 1);
	} else if (!IS_ENABLED(CONFIG_KARO_UBOOT_MFG)) {
		karo_fdt_move_fdt();
		rand_init();
		show_bmp_logo();
	}
	txmp_set_bootdevice();

	clear_ctrlc();
	return 0;
}

void board_quiesce_devices(void)
{
}

#if defined(CONFIG_USB_DWC3) && defined(CONFIG_CMD_STM32PROG_USB)
#include <dfu.h>
/*
 * TEMP: force USB BUS reset forced to false, because it is not supported
 *       in DWC3 USB driver
 * avoid USB bus reset support in DFU stack is required to reenumeration in
 * stm32prog command after flashlayout load or after "dfu-util -e -R"
 */
bool dfu_usb_get_reset(void)
{
	return false;
}
#endif

/* weak function called from common/board_r.c */
int is_flash_available(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_MTD,
					  DM_DRIVER_GET(stm32_hyperbus),
					  &dev);
	return !ret;
}

/* weak function called from env/sf.c */
void *env_sf_get_env_addr(void)
{
	return NULL;
}
