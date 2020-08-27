// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <env.h>
#include <env_internal.h>
#include <malloc.h>
#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
#if defined(CONFIG_KARO_TX8MM_1610)
#define KARO_BOARD_NAME		"tx8m-1610"
#elif defined(CONFIG_KARO_TX8MM_1620)
#define KARO_BOARD_NAME		"tx8m-1620"
#elif defined(CONFIG_KARO_TX8MN)
#define KARO_BOARD_NAME		"tx8m-nd00"
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
	set_default_env(NULL);
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
#ifdef CONFIG_ENV_CALLBACK_LIST_DEFAULT
	if (!env_get(ENV_CALLBACK_VAR))
		env_set(ENV_CALLBACK_VAR, CONFIG_ENV_CALLBACK_LIST_DEFAULT);
#endif
	karo_set_part_uuids();
	karo_env_set_uboot_vars();
}
#endif

/*
 * Callback function to insert/remove the 'baseboard' name into/from
 * the 'fdt_file' variable whenever the 'baseboard' variable is changed.
 */
static int karo_env_baseboard(const char *name, const char *value,
			      enum env_op op, int flags)
{
	const char *fdt_file;
	char *dlm;
	int len;
	char *new_fdt_file;
	size_t alloc_len;
	const char *ext = ".dtb";
	size_t ext_len = strlen(ext);

	if (!(flags & H_INTERACTIVE))
		return 0;

	fdt_file = env_get("fdt_file");
	if (!fdt_file)
		return 0;

	if (strcmp(fdt_file + strlen(fdt_file) - ext_len, ext) != 0)
		ext_len = 0;

	/*
	 * Skip over the first two dashes in fdt_file name:
	 * "<soc>-<module>-<version>[-<baseboard>][.dtb]"
	 *                           ^
	 * If the fdt_file name does not match the above
	 * pattern it won't be changed.
	 */
	dlm = strchr(fdt_file, '-');
	if (!dlm)
		return 0;
	dlm = strchr(dlm + 1, '-');
	if (!dlm)
		return 0;
	dlm = strchr(dlm + 1, '-');
	if (dlm)
		len = dlm - fdt_file;
	else
		len = strlen(fdt_file) - ext_len;

	if (op == env_op_delete) {
		char *olddtb;

		if (!dlm)
			return 0;

		olddtb = strdup(fdt_file);
		if (ext_len)
			strcpy(dlm, ext);
		else
			*dlm = '\0';

		pr_notice("Notice: 'fdt_file' changed from '%s' to '%s'\n",
			  olddtb, fdt_file);
		env_set("fdt_file", fdt_file);
		free(olddtb);
		return 0;
	}

	alloc_len = len + strlen(value) + ext_len + 2;
	new_fdt_file = malloc(alloc_len);
	if (!new_fdt_file)
		return 0;

	strncpy(new_fdt_file, fdt_file, len);
	new_fdt_file[len] = '-';
	/* append new 'baseboard' value */
	strncpy(new_fdt_file + len + 1, value, strlen(value));
	if (ext_len)
		strcpy(new_fdt_file + alloc_len - 1 - ext_len, ext);
	else
		new_fdt_file[alloc_len - 1 - ext_len] = '\0';

	if (strcmp(fdt_file, new_fdt_file) != 0) {
		printf("Notice: 'fdt_file' changed from '%s' to '%s'\n",
		       fdt_file, new_fdt_file);
		env_set("fdt_file", new_fdt_file);
	}
	free(new_fdt_file);
	return 0;
}

U_BOOT_ENV_CALLBACK(baseboard, karo_env_baseboard);
