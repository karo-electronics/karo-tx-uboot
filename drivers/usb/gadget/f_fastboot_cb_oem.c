/*
 * Copyright (C) 2016 GlobalLogic
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <errno.h>
#include <version.h>
#include <malloc.h>
#include <part.h>
#include <fastboot.h>
#include <part.h>
#include <div64.h>
#include <android/bootloader.h>
#include <linux/ctype.h>
#include <env_internal.h>
#include <fdtdec.h>
#include <dm/ofnode.h>
#include <cli.h>

#ifndef CONFIG_RANDOM_UUID
#error "CONFIG_RANDOM_UUID must be enabled for oem partitioning"
#endif

static int oem_dump_help(char *response)
{
	fastboot_send_response("INFO  flash <IPL>  - flash specified IPL:");
	fastboot_send_response("INFO");
	fastboot_send_response("INFO  format	   - create new GPT partition table on eMMC.");
	fastboot_send_response("INFO  lock_status  - show lock status of the IPL and eMMC.");
	fastboot_send_response("INFO  setenv NAME <VALUE>  - set environment variable.\n");
	fastboot_send_response("INFO  setenv default	   - set environment variables to default.\n");
	fastboot_okay(NULL, response);
	return 0;
}

static int oem_format(char *response)
{
	struct blk_desc *blkdev = blk_get_dev("mmc", CONFIG_FASTBOOT_FLASH_MMC_DEV);
	struct disk_partition *parts;
	int i, node, part_subnode, num_parts;
	char uuid[64], name[32];
	const void *fdt = gd->fdt_blob;

	node = fdt_path_offset(fdt, ANDROID_PARTITIONS_PATH);
	num_parts = fdtdec_get_child_count(fdt, node);

	if (!blkdev || blkdev->type == DEV_TYPE_UNKNOWN) {
		fastboot_fail("FAIL eMMC device not found!", response);
		return -1;
	}

	fastboot_send_response("INFO eMMC: %s %s %s %zuMiB",
		blkdev->vendor, blkdev->product, blkdev->revision,
		(blkdev->lba * blkdev->blksz) / 1024 / 1024);

	parts = calloc(num_parts, sizeof(struct disk_partition));
	if (parts == NULL) {
		printf("%s: Unable to allocate memory for new partition table, %lu bytes",
			__func__, num_parts * sizeof(struct disk_partition));
		return -1;
	}

	/* Set offset for first partition, reserved space 512KiB */
	parts[0].start = (524288 / blkdev->blksz);

	/* Construct partition table */
	i = 0;
	fdt_for_each_subnode(part_subnode, fdt, node)
	{
		size_t blks, size = fdtdec_get_uint64(fdt, part_subnode, "size", -1);

		/* Calculate size of partition in blocks */
		blks = (size / blkdev->blksz);
		if ((size % blkdev->blksz) != 0) {
			blks++;
		}
		parts[i].size = blks;

		/* Set partition UUID */
		gen_rand_uuid_str(parts[i].uuid, UUID_STR_FORMAT_STD);

		ofnode part_ofnode = offset_to_ofnode(part_subnode);
		/* Copy partition name */
		strcpy((char*) parts[i].name, ofnode_get_property(part_ofnode, "title", NULL));

		const char *part_slot = ofnode_get_property(part_ofnode, "slot", NULL);
		/* Append slot if exist */
		if (part_slot != NULL) {
			strcat((char*) parts[i].name, part_slot);
		}
		i++;
	}

	/* Gen disk UUID */
	gen_rand_uuid_str(uuid, UUID_STR_FORMAT_STD);

	/* Save partitions layout to disk */
	if (gpt_restore(blkdev, uuid, parts, num_parts) < 0) {
		fastboot_fail(" Save partitions layout to disk failed", response);
		free(parts);
		return -1;
	}

	free(parts);

	fastboot_send_response("INFO Created new GPT partition table:");

	fdt_for_each_subnode(part_subnode, fdt, node)
	{
		struct disk_partition info;

		ofnode part_ofnode = offset_to_ofnode(part_subnode);

		const char *part_name = ofnode_get_property(part_ofnode, "title", NULL);
		strcpy(name, part_name);

		const char *part_slot = ofnode_get_property(part_ofnode, "slot", NULL);
		/* Append slot if exist */
		if (part_slot != NULL) {
			strcat(name, part_slot);
		}

		const char *part_fs = ofnode_get_property(part_ofnode, "type", NULL);
		if (part_get_info_by_name(blkdev, name, &info) >= 0) {
		fastboot_send_response("INFO     /%s (%zu KiB, %s)",
			info.name, (info.size * blkdev->blksz) / 1024, part_fs);
		} else {
			fastboot_send_response("INFO     /%s (ERROR unable to get info)",
					part_name);
		}
	}

	fastboot_okay(NULL, response);
	return 0;
}

static int oem_lock_status(char *response)
{
	int ipl_locked = 0, mmc_locked = 0;

	if (!fastboot_get_lock_status(&mmc_locked, &ipl_locked)) {
		fastboot_send_response("INFO IPL %s", ipl_locked ? "locked" : "unlocked");
		fastboot_send_response("INFO MMC %s", mmc_locked ? "locked" : "unlocked");
	}
	fastboot_okay(NULL, response);
	return 0;
}

static int oem_setenv(char * varval, char *response)
{
	const char * key;
	char finaltoken[CONFIG_SYS_CBSIZE] = {0};

	if (!strcmp(varval, "default")) {
		env_set_default("## Resetting to default environmens \n", 0);
		if (env_save()) {
			fastboot_fail("Save environment variable error", response);
			return -1;
		}
		/* Set proper platform in environment */
		rzg2_preset_env();
		fastboot_okay(NULL, response);
		return 0;
	}

	key = strsep(&varval, " ");
	if (!key) {
		fastboot_fail("You must specify the name of the environment variable", response);
		return -1;
	}

	cli_simple_process_macros(varval, finaltoken);
	if (varval && *finaltoken != '\0') {
		fastboot_send_response("INFOsetting env %s", key);
	} else {
		fastboot_send_response("INFOerase env %s", key);
	}
	env_set(key, finaltoken);
	if (env_save()) {
		fastboot_response("FAIL", response, "Save environment variable '%s' error", key);
		return -1;
	}
	fastboot_okay(NULL, response);
	return 0;
}

void fastboot_cb_oem(char *cmd, char *response)
{
	int ipl_locked = 0, mmc_locked = 0;
	char *cmdarg;

	strsep(&cmd, " ");
	cmdarg = cmd;
	strsep(&cmdarg, " ");
	if (!strcmp(cmd, "help")) {
		oem_dump_help(response);
		return;
	}

	if (!strcmp(cmd, "lock_status")) {
		oem_lock_status(response);
		return;
	}

	fastboot_get_lock_status(&mmc_locked, &ipl_locked);

	if (!strcmp(cmd, "format")) {
		if (mmc_locked || ipl_locked) {
			fastboot_fail("flash device locked", response);
			return;
		}
		oem_format(response);
		return;
	}

	if (!strcmp(cmd, "setenv")) {
		if (cmdarg == NULL) {
			fastboot_fail("oem setenv command requires one or two arguments", response);
			return;
		}
		oem_setenv(cmdarg, response);
		return;
	}
	fastboot_response("FAIL", response, " unsupported oem command: %s", cmd);
}
