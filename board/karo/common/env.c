// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <env.h>
#include <env_internal.h>
#include <malloc.h>
#include <part.h>
#include <usb.h>
#include <asm/bootm.h>
#include <asm/global_data.h>
#include <asm/setup.h>
#include <dm/ofnode.h>
#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
#if defined(CONFIG_KARO_QSMP_1351)
#define KARO_BOARD_NAME		"qsmp-1351"
#elif defined(CONFIG_KARO_QSMP_1530)
#define KARO_BOARD_NAME		"qsmp-1530"
#elif defined(CONFIG_KARO_QSMP_1570)
#define KARO_BOARD_NAME		"qsmp-1570"
#elif defined(CONFIG_KARO_TXMP_1570)
#define KARO_BOARD_NAME		"txmp-1570"
#elif defined(CONFIG_KARO_TXMP_1571)
#define KARO_BOARD_NAME		"txmp-1571"
#elif defined(CONFIG_KARO_QSMP_2030)
#define KARO_BOARD_NAME		"qsmp-2030"
#elif defined(CONFIG_KARO_QSMP_2350)
#define KARO_BOARD_NAME		"qsmp-2350"
#elif defined(CONFIG_KARO_QSMP_2550)
#define KARO_BOARD_NAME		"qsmp-2550"
#elif defined(CONFIG_KARO_TXMP_2550)
#define KARO_BOARD_NAME		"txmp-2550"
#elif defined(CONFIG_KARO_TXMP_2570)
#define KARO_BOARD_NAME		"txmp-2570"
#else
#error Unsupported module variant
#endif

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
__weak void board_debug_uart_init(void)
{
}
#endif

static void karo_env_set_uboot_vars(void)
{
	const void *fdt_compat;
	int fdt_compat_len;
	const char *vendor_pfx = "karo,";
	const int pl = strlen(vendor_pfx);

	fdt_compat = ofnode_get_property(ofnode_root(), "compatible",
					 &fdt_compat_len);
	if (fdt_compat && fdt_compat_len) {
		if (strncmp(fdt_compat, vendor_pfx, pl) != 0)
			env_set("board_name", fdt_compat);
		else
			env_set("board_name", fdt_compat + pl);
	} else {
		env_set("board_name", KARO_BOARD_NAME);
	}

	if (IS_ENABLED(CONFIG_KARO_UBOOT_MFG))
		env_set("board_rev", "mfg");
	else if (IS_ENABLED(CONFIG_UBOOT_IGNORE_ENV))
		env_set("board_rev", "noenv");
	else
		env_set("board_rev", "default");

	if (!env_get("soc_prefix"))
		env_set("soc_prefix", SOC_PREFIX);
	if (!env_get("soc_family"))
		env_set("soc_family", SOC_FAMILY);
}
#else
static inline void karo_env_set_uboot_vars(void)
{
}
#endif /* CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG */

#ifdef CONFIG_UBOOT_IGNORE_ENV
void karo_env_cleanup(void)
{
	printf("Using default environment\n");
	env_set_default(NULL, 0);
	karo_env_set_uboot_vars();
}
#else
static const char * const cleanup_vars[] = {
#ifdef CONFIG_KARO_UBOOT_MFG
	"bootdelay",
#endif
	"bootargs",
	"fileaddr",
	"filesize",
	"safeboot",
	"wdreset",
};

#if defined(CONFIG_KARO_STM32_EMMC) && !defined(CONFIG_KARO_UBOOT_MFG)
static void karo_set_part_uuids(void)
{
	int ret;
	struct blk_desc *dev_desc;
	struct disk_partition info;
	int partno;
	char part_uuid_name[PART_NAME_LEN + 5]; /* "uuid_${part_name}" */
	const char *bootpart = env_get("bootpart");
	const char *dev_type = env_get("bootdev");
	const char *cur_uuid;
	int devno;

	if (!dev_type)
		return;

	if (bootpart)
		devno = strtoul(bootpart, NULL, 10);
	else
		devno = 0;

	if (CONFIG_IS_ENABLED(USB_STORAGE) &&
	    strcmp(dev_type, "usb") == 0) {
		ret = usb_init();
		if (ret)
			return;
	}

	dev_desc = blk_get_devnum_by_typename(dev_type, devno);
	if (!dev_desc)
		return;

	for (partno = 1; partno < MAX_SEARCH_PARTITIONS; partno++) {
		ret = part_get_info(dev_desc, partno, &info);
		if (ret != 0)
			continue;

		ret = snprintf(part_uuid_name, sizeof(part_uuid_name),
			       "uuid_%s", info.name);
		if (ret >= sizeof(part_uuid_name)) {
			printf("buffer overflow for variable name 'uuid_%s'; need %u characters, got only %zu\n",
			       info.name, ret, sizeof(part_uuid_name));
			continue;
		}
		cur_uuid = env_get(part_uuid_name);
		if (cur_uuid && strcmp(cur_uuid, info.uuid) == 0) {
			debug("${%s} matches the %s partition UUID: %s\n",
			      part_uuid_name, info.name, info.uuid);
			continue;
		}
		if (!cur_uuid)
			debug("Setting %s='%s'\n", part_uuid_name, info.uuid);
		else
			debug("Changing '%s' from\n%s to\n%s\n",
			      part_uuid_name, cur_uuid, info.uuid);
		env_set(part_uuid_name, info.uuid);
	}
}
#else
static inline void karo_set_part_uuids(void)
{
}
#endif

void karo_env_cleanup(void)
{
	size_t i;

	debug("%s@%d: Cleaning up environment\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(cleanup_vars); i++) {
		if (env_get(cleanup_vars[i])) {
			debug("Deleting %s='%s'\n", cleanup_vars[i], env_get(cleanup_vars[i]));
			env_set(cleanup_vars[i], NULL);
		}
	}
	karo_set_part_uuids();
	karo_env_set_uboot_vars();
}
#endif
