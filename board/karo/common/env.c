// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <env.h>
#include <env_internal.h>
#include <malloc.h>
#include <asm/bootm.h>
#include <asm/setup.h>
#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
#if defined(CONFIG_KARO_TX8MM_1610)
#define KARO_BOARD_NAME		"tx8m-1610"
#define SOC_FAMILY		"imx8mm"
#elif defined(CONFIG_KARO_TX8MM_1620)
#define KARO_BOARD_NAME		"tx8m-1620"
#define SOC_FAMILY		"imx8mm"
#elif defined(CONFIG_KARO_TX8MM_1622)
#define KARO_BOARD_NAME		"tx8m-1622"
#define SOC_FAMILY		"imx8mm"
#elif defined(CONFIG_KARO_TX8MN)
#define KARO_BOARD_NAME		"tx8m-nd00"
#define SOC_FAMILY		"imx8mn"
#elif defined(CONFIG_KARO_QS8M_MQ00)
#define KARO_BOARD_NAME		"qs8m-mq00"
#define SOC_FAMILY		"imx8mm"
#elif defined(CONFIG_KARO_QS8M_ND00)
#define KARO_BOARD_NAME		"qs8m-nd00"
#define SOC_FAMILY		"imx8mn"
#elif defined(CONFIG_KARO_QSXM_MM60)
#define KARO_BOARD_NAME		"qsxm-mm60"
#define SOC_FAMILY		"imx8mm"
#elif defined(CONFIG_KARO_QSXP_ML81)
#define KARO_BOARD_NAME		"qsxp-ml81"
#define SOC_FAMILY		"imx8mp"
#elif defined(CONFIG_KARO_TX8P_ML81)
#define KARO_BOARD_NAME		"tx8p-ml81"
#define SOC_FAMILY		"imx8mp"
#elif defined(CONFIG_KARO_TX8P_ML82)
#define KARO_BOARD_NAME		"tx8p-ml82"
#define SOC_FAMILY		"imx8mp"
#else
#error Unsupported module variant
#endif

static void karo_env_set_uboot_vars(void)
{
	env_set("board_name", KARO_BOARD_NAME);
#if defined(CONFIG_KARO_UBOOT_MFG)
	env_set("board_rev", "mfg");
#elif defined(CONFIG_UBOOT_IGNORE_ENV)
	env_set("board_rev", "noenv");
#else
	env_set("board_rev", "default");
#endif
	if (!env_get("soc_prefix"))
		env_set("soc_prefix", "imx8m");
	if (!env_get("soc_family"))
		env_set("soc_family", SOC_FAMILY);

	if (!env_get("serial#")) {
		struct tag_serialnr serno;
		char serno_str[sizeof(serno) * 2 + 1];

		get_board_serial(&serno);
		snprintf(serno_str, sizeof(serno_str), "%08x%08x",
			 serno.high, serno.low);
		env_set("serial#", serno_str);
	}
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

#if !defined(CONFIG_KARO_UBOOT_MFG)
static void karo_set_part_uuids(void)
{
	int ret;
	struct blk_desc *dev_desc;
	struct disk_partition info;
	int partno;
	char dev_part_str[16];
	char part_uuid_name[PART_NAME_LEN + 5]; /* "uuid_${part_name}" */
	const char *mmcdev = env_get("mmcdev");
	const char *cur_uuid;

	debug("%s@%d:\n", __func__, __LINE__);

	if (!mmcdev)
		mmcdev = "0";

	ret = blk_get_device_by_str("mmc", mmcdev, &dev_desc);
	if (ret < 0)
		return;

	for (partno = 1; partno < MAX_SEARCH_PARTITIONS; partno++) {
		ret = snprintf(dev_part_str, sizeof(dev_part_str), "%s:%u",
			       mmcdev, partno);
		if (ret >= sizeof(dev_part_str)) {
			printf("Invalid mmcdev '%s'\n", env_get("mmcdev"));
			break;
		}
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
		if (!env_get(cleanup_vars[i]))
			env_set(cleanup_vars[i], NULL);
	}
	karo_set_part_uuids();
	karo_env_set_uboot_vars();
}
#endif