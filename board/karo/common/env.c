// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <env.h>
#include <env_callback.h>
#include <log.h>
#include <malloc.h>
#include <part.h>
#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_UBOOT_IGNORE_ENV
void env_cleanup(void)
{
	debug("%s@%d:\n", __func__, __LINE__);
	env_set_default(NULL, 0);
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

#if defined(CONFIG_TXMP_EMMC) && !defined(CONFIG_KARO_UBOOT_MFG)
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
			printf("buffer overflow for variable name 'uuid_%s'; need %u characters, got only %u\n",
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

void env_cleanup(void)
{
	size_t i;

	debug("%s@%d:\n", __func__, __LINE__);
	for (i = 0; i < ARRAY_SIZE(cleanup_vars); i++) {
		debug("Clearing '%s'\n", cleanup_vars[i]);
		env_set(cleanup_vars[i], NULL);
	}
#ifdef CONFIG_ENV_CALLBACK_LIST_DEFAULT
	if (!env_get(ENV_CALLBACK_VAR))
		env_set(ENV_CALLBACK_VAR, CONFIG_ENV_CALLBACK_LIST_DEFAULT);
#endif
	karo_set_part_uuids();
}
#endif

/*
 * Callback function to insert/remove the 'baseboard' name into/from
 * the 'dtbfile' variable whenever the 'baseboard' variable is changed.
 */
static int karo_env_baseboard(const char *name, const char *value,
			      enum env_op op, int flags)
{
	const char *dtbfile;
	char *dlm;
	int len;
	char *new_dtbfile;
	size_t alloc_len;
	const char *ext = ".dtb";
	size_t ext_len = strlen(ext);

	debug("%s@%d:\n", __func__, __LINE__);
	if (!(flags & H_INTERACTIVE))
		return 0;

	dtbfile = env_get("dtbfile");
	if (!dtbfile)
		return 0;

	if (strcmp(dtbfile + strlen(dtbfile) - ext_len, ext) != 0)
		ext_len = 0;

	/*
	 * Skip over the first two dashes in dtbfile name:
	 * "<soc>-<module>-<version>[-<baseboard>][.dtb]"
	 *                           ^
	 * If the dtbfile name does not match the above
	 * pattern it won't be changed.
	 */
	dlm = strchr(dtbfile, '-');
	if (!dlm)
		return 0;
	dlm = strchr(dlm + 1, '-');
	if (!dlm)
		return 0;
	dlm = strchr(dlm + 1, '-');
	if (dlm)
		len = dlm - dtbfile;
	else
		len = strlen(dtbfile) - ext_len;

	if (op == env_op_delete) {
		char *olddtb;

		if (!dlm)
			return 0;

		olddtb = strdup(dtbfile);
		if (ext_len)
			strcpy(dlm, ext);
		else
			*dlm = '\0';

		pr_notice("Notice: 'dtbfile' changed from '%s' to '%s'\n",
			  olddtb, dtbfile);
		env_set("dtbfile", dtbfile);
		free(olddtb);
		return 0;
	}

	alloc_len = len + strlen(value) + ext_len + 2;
	new_dtbfile = malloc(alloc_len);
	if (!new_dtbfile)
		return 0;

	strncpy(new_dtbfile, dtbfile, len);
	new_dtbfile[len] = '-';
	/* append new 'baseboard' value */
	strncpy(new_dtbfile + len + 1, value, strlen(value));
	if (ext_len)
		strcpy(new_dtbfile + alloc_len - 1 - ext_len, ext);
	else
		new_dtbfile[alloc_len - 1 - ext_len] = '\0';

	if (strcmp(dtbfile, new_dtbfile) != 0) {
		printf("Notice: 'dtbfile' changed from '%s' to '%s'\n",
		       dtbfile, new_dtbfile);
		env_set("dtbfile", new_dtbfile);
	}
	free(new_dtbfile);
	return 0;
}

U_BOOT_ENV_CALLBACK(baseboard, karo_env_baseboard);
