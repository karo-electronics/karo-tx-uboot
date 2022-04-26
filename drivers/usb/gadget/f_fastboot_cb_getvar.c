/*
 * Copyright (C) 2018 GlobalLogic
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <version.h>
#include <fastboot.h>
#include <avb_verify.h>
#include <dm/ofnode.h>
#include <env.h>

struct var_dispatch_info {
	char *var;
	int (*cb)(char *, int);
};


/* Initialized in fastboot_cb_getvar() */
static struct oem_part_table oem_partition_table;

/* ------------------------------------------------------------------ */
static int cb_version_fastboot(char *buf, int buf_size)
{
	return snprintf(buf, buf_size, FASTBOOT_VERSION);
}

static int cb_version_baseband(char *buf, int buf_size)
{
	return snprintf(buf, buf_size, "N/A");
}

static int cb_revision(char *buf, int buf_size)
{
	u32 rev_integer = rmobile_get_cpu_rev_integer();
	u32 rev_fraction = rmobile_get_cpu_rev_fraction();

	return snprintf(buf, buf_size, "%d.%d", rev_integer, rev_fraction);
}

static int cb_version_bootloader(char *buf, int buf_size)
{
	return snprintf(buf, buf_size, U_BOOT_VERSION);
}

static int cb_product(char *buf, int buf_size)
{
	const char * env = env_get("product");

	if(env) {
		return snprintf(buf, buf_size, "%s", env);
	}
	return 0;
}

static int cb_platform(char *buf, int buf_size)
{
	const char * env = env_get("platform");

	if(env) {
		return snprintf(buf, buf_size, "%s", env);
	}
	return 0;
}

static int cb_serialno(char *buf, int buf_size)
{
	const char * env = env_get("serialno");

	if(env) {
		return snprintf(buf, buf_size, "%s", env);
	}
	return 0;
}

static int cb_variant(char *buf, int buf_size)
{
	buf[0] = 0; /* Empty */
	return 0;
}
static int cb_current_slot(char *buf, int buf_size)
{
	const char *slot = cb_get_slot_char();

	memset(buf, 0, sizeof(buf_size));
	if(slot) {
		return snprintf(buf, buf_size, "%s", slot);
	}
	return 0;
}

static int cb_slot_suffixes(char *buf, int buf_size)
{
	return snprintf(buf, buf_size,
#ifdef ANDROID_MMC_ONE_SLOT
	"a");
#else
	"a,b");
#endif
}

static int cb_slot_count(char *buf, int buf_size)
{
	return snprintf(buf, buf_size,
#ifdef ANDROID_MMC_ONE_SLOT
	"1");
#else
	"2");
#endif
}

static int cb_secure(char *buf, int buf_size)
{
	return snprintf(buf, buf_size, "no");
}

static int cb_unlocked(char *buf, int buf_size)
{
	int ipl_locked = 0, mmc_locked = 0;
	char * yesno = "yes";

	if (!fastboot_get_lock_status(&mmc_locked, &ipl_locked)) {
		if (mmc_locked) {
			yesno = "no";
		}
	}
	return snprintf(buf, buf_size, yesno);
}

static int cb_max_download_size(char *buf, int buf_size)
{
	return snprintf(buf, buf_size, "0x%x", CONFIG_FASTBOOT_BUF_SIZE);
}

/* Returns index of partition descriptor, of found, otherwise -1 */
static int find_oem_partition_with_slot(char *partname)
{
	const char *cur_slot = cb_get_slot_char();

	for (int i = 0; i < oem_partition_table.parts_count; i++) {
		if (cur_slot != NULL && oem_partition_table.partitions[i].slot != NULL) {
			if (!strcmp(cur_slot, oem_partition_table.partitions[i].slot) &&
				!strcmp(partname, oem_partition_table.partitions[i].name)) {
					return i;
				} else {
					char name[32];
					snprintf(name, sizeof(name), "%s%s",
						oem_partition_table.partitions[i].name,
						oem_partition_table.partitions[i].slot);

					if (!strcmp(partname, name))
						return i;
				}
		}
	}
	return -1;
}

static int cb_partition_size(char *buf, int buf_size)
{
    struct blk_desc *blkdev;
    struct disk_partition info;

	char *cmd = strtok(buf,   ":");
	char *part = strtok(NULL, ":");
	char name[32];
	int i;

	blkdev = blk_get_dev("mmc", CONFIG_FASTBOOT_FLASH_MMC_DEV);
	if (!blkdev || blkdev->type == DEV_TYPE_UNKNOWN) {
		printf("%s: MMC device not found!\n", __func__);
		return -1;
	}
	/* For 'getvar all' */
	if (!strcmp(buf, "all")) {
		for (i = 0; i < oem_partition_table.parts_count; i++) {
			if (oem_partition_table.partitions[i].slot != NULL) {
				snprintf(name, sizeof(name), "%s%s",
					oem_partition_table.partitions[i].name,
					oem_partition_table.partitions[i].slot);
			} else {
				strcpy(name, oem_partition_table.partitions[i].name);
			}
			if (part_get_info_by_name(blkdev, name, &info) < 0) {
				printf("%s: Can't find MMC partition '%s (%s)'\n",
				          __func__, part, name);
				return -1;
			}
			fastboot_send_response("INFOpartition-size:%s:0x%zx",
					name, info.size * info.blksz);
		}
		return -1;
	}
	if (cmd != NULL && part != NULL) {
		strcpy(name, part);
		if ((i = find_oem_partition_with_slot(part)) >= 0) {
			snprintf(name, sizeof(name), "%s%s",
				oem_partition_table.partitions[i].name,
				oem_partition_table.partitions[i].slot);
		}
		if (part_get_info_by_name(blkdev, name, &info) < 0) {
			printf("%s: Can't find MMC partition '%s (%s)'\n", __func__, part, name);
			return -1;
		}
		return snprintf(buf, buf_size, "0x%zx", info.size * info.blksz);
	}
	return -1;
}

static int cb_partition_type(char *buf, int buf_size)
{
	char *cmd = strtok(buf,   ":");
	char *part = strtok(NULL, ":");
	int i;

	/* For 'getvar all' */
	if (!strcmp(buf, "all")) {
		for (i = 0; i < oem_partition_table.parts_count; i++) {
			if (oem_partition_table.partitions[i].slot != NULL) {
				fastboot_send_response("INFOpartition-type:%s%s:%s",
						oem_partition_table.partitions[i].name,
						oem_partition_table.partitions[i].slot,
						oem_partition_table.partitions[i].fs);
			} else {
				fastboot_send_response("INFOpartition-type:%s:%s",
						oem_partition_table.partitions[i].name,
						oem_partition_table.partitions[i].fs);
			}
		}
		return -1;
	}

	if(cmd != NULL && part != NULL)
	{
		i = find_oem_partition_with_slot(part);

		if (i < 0) {
			for (i = 0; i < oem_partition_table.parts_count; i++) {
				if (!strcmp(part, oem_partition_table.partitions[i].name)) {
					break;
				}
			}
		}

		if (i < oem_partition_table.parts_count) {
			return snprintf(buf, buf_size, "%s",
					oem_partition_table.partitions[i].fs);
		}
	}
	return -1;
}

static int cb_has_slot(char *buf, int buf_size)
{
	char *cmd = strtok(buf,   ":");
	char *part = strtok(NULL, ":");

	if (!strcmp(buf, "all")) {
		for (int i = 0; i < oem_partition_table.parts_count; i++) {
			if (oem_partition_table.partitions[i].slot != NULL) {
				/*Report slot only once for partition *_a */
				if (!strcmp(oem_partition_table.partitions[i].slot, "_a")) {
					fastboot_send_response("INFOhas-slot:%s:%s",
							oem_partition_table.partitions[i].name,
							"yes");
				}
			} else {
				fastboot_send_response("INFOhas-slot:%s:%s",
						oem_partition_table.partitions[i].name, "no");
			}
		}
		return -1;
	}

	if(cmd != NULL && part != NULL) {
		for (int i = 0; i < oem_partition_table.parts_count; i++) {
			if (!strcmp(oem_partition_table.partitions[i].name, part)) {
				return snprintf(buf, buf_size, "%s",
				oem_partition_table.partitions[i].slot != NULL ? "yes" : "no");
			}
		}
	}

	return snprintf(buf, buf_size, "no");
}


#define MAX_SLOTS   2
static int cb_slot_successful(char *buf, int buf_size)
{
	int ret, slot;
	bool succesful;
	char *cmd = strtok(buf,   ":");
	char *slot_char = strtok(NULL, ":");

	if (!strcmp(buf, "all")) {
		for (int i = 0; i < MAX_SLOTS; i++) {
			ret = avb_is_slot_successful(i, &succesful);
			if (!ret) {
				fastboot_send_response("INFOslot-successful:%s:%s",
						i ? "b" : "a" , succesful ? "yes" : "no");
			}
		}
		return -1;
	}
	if(cmd != NULL && slot_char != NULL) {
		if (!strcmp(slot_char, "a")) {
			slot = 0;
		} else if (!strcmp(slot_char, "b")){
			slot = 1;
		} else {
			return -1;
		}
		ret = avb_is_slot_successful(slot, &succesful);
		if (!ret) {
			return snprintf(buf, buf_size, succesful ? "yes" : "no");
		}
	}
	buf[0] = 0; /* Empty */
	return 0;
}

static int cb_slot_unbootable(char *buf, int buf_size)
{
	int ret, slot;
	bool bootable;
	char *cmd = strtok(buf,   ":");
	char *slot_char = strtok(NULL, ":");

	if (!strcmp(buf, "all")) {
		for (int i = 0; i < MAX_SLOTS; i++) {
			ret = avb_is_slot_bootable(i, &bootable);
			if (!ret) {
			fastboot_send_response("INFOslot-unbootable:%s:%s",
				i ? "b" : "a" ,bootable ? "no" : "yes");
			}
		}
		return -1;
	}
	if(cmd != NULL && slot_char != NULL) {
		if (!strcmp(slot_char, "a")) {
			slot = 0;
		} else if (!strcmp(slot_char, "b")){
			slot = 1;
		} else {
			return -1;
		}
		ret = avb_is_slot_bootable(slot, &bootable);
		if (!ret) {
			return snprintf(buf, buf_size, bootable ? "no" : "yes");
		}
	}
	buf[0] = 0; /* Empty */
	return 0;
}

static int cb_slot_retry_count(char *buf, int buf_size)
{
	int slot;
	int retry_cnt;
	char *cmd = strtok(buf,   ":");
	char *slot_char = strtok(NULL, ":");

	if (!strcmp(buf, "all")) {
		for (int i = 0; i < MAX_SLOTS; i++) {
			retry_cnt = avb_get_slot_retry(i);
			if (retry_cnt >= 0) {
				fastboot_send_response("INFOslot-retry-count:%s:%u",
					i ? "b" : "a" , retry_cnt);
			}
		}
		return -1;
	}
	if(cmd != NULL && slot_char != NULL) {
		if (!strcmp(slot_char, "a")) {
			slot = 0;
		} else if (!strcmp(slot_char, "b")){
			slot = 1;
		} else {
			return -1;
		}
		retry_cnt = avb_get_slot_retry(slot);
		if (retry_cnt >= 0) {
			return snprintf(buf, buf_size, "%u", retry_cnt);
		}
	}
	buf[0] = 0; /* Empty */
	return 0;
}

static int cb_is_userspace(char *buf, int buf_size)
{
	return snprintf(buf, buf_size, "no");
}

/* ------------------------------------------------------------------ */
static const struct var_dispatch_info var_table[] =
{
	{ "version-baseband",       cb_version_baseband },
	{ "version-bootloader",     cb_version_bootloader },
	{ "version",                cb_version_fastboot },
	{ "product",                cb_product },
	{ "platform",               cb_platform },
	{ "revision",               cb_revision },
	{ "serialno",               cb_serialno },
	{ "variant",                cb_variant },
	{ "secure",                 cb_secure },
	{ "unlocked",               cb_unlocked },
	{ "max-download-size",      cb_max_download_size },
	{ "partition-size:",        cb_partition_size },
	{ "partition-type:",        cb_partition_type },
	{ "has-slot:",              cb_has_slot },
	{ "slot-suffixes",          cb_slot_suffixes },
	{ "slot-count",             cb_slot_count },
	{ "current-slot",           cb_current_slot },
	{ "slot-successful:",       cb_slot_successful },
	{ "slot-unbootable:",       cb_slot_unbootable },
	{ "slot-retry-count:",      cb_slot_retry_count },
	{ "is-userspace",           cb_is_userspace }
};

void fastboot_cb_getvar(const char *var, char *response)
{
	int i, found;
	int node, subnode;
	char val[FASTBOOT_RESPONSE_LEN];

	const void *fdt = gd->fdt_blob;
	node = fdt_path_offset(fdt, ANDROID_PARTITIONS_PATH);
	oem_partition_table.parts_count = fdtdec_get_child_count(fdt, node);

	oem_partition_table.partitions = calloc(oem_partition_table.parts_count,
						sizeof(struct oem_part_info));
	i = 0;
	fdt_for_each_subnode(subnode, fdt, node) {
		oem_partition_table.partitions[i].size = fdtdec_get_uint64(fdt, subnode,
									"size", -1);

		ofnode part_ofnode = offset_to_ofnode(subnode);
		oem_partition_table.partitions[i].name = ofnode_get_property(part_ofnode,
									"title", NULL);
		oem_partition_table.partitions[i].slot = ofnode_get_property(part_ofnode,
									"slot", NULL);
		oem_partition_table.partitions[i].fs = ofnode_get_property(part_ofnode,
									"type", NULL);

		i++;
	}
	if (!strcmp(var, "all")) {
		for (i = 0; i < ARRAY_SIZE(var_table); i++) {
			strcpy(val, var);
			if (var_table[i].cb(val, sizeof(val)-1) >= 0) {
				fastboot_send_response("INFO%s:%s", var_table[i].var, val);
			}
		}
		fastboot_okay("listed above", response);
		goto free_oem_partitions;
	}

	for (i = 0, found = 0; i < ARRAY_SIZE(var_table); i++) {
		/* If variable name contains ':' check partially */
		if (strstr(var_table[i].var, ":") != NULL) {
			if (!strncmp(var, var_table[i].var, strlen(var_table[i].var))) {
				found = 1;
			}
		} else { /* otherwise check all chars */
			if (!strcmp(var, var_table[i].var)) {
				found = 1;
			}
		}

		if (found) {
			strcpy(val, var);
			if (var_table[i].cb(val, sizeof(val)-1) >= 0) {
				fastboot_response("OKAY", response, "%s", val);
				goto free_oem_partitions;
			}
			break;
		}
	}

	/* Fastboot specific variable not found?
	 * Looking for global bootloader variable */
	const char *s = env_get(var);

	if(s) {
		fastboot_response("OKAY", response, "%s", s);
		goto free_oem_partitions;
	}

	fastboot_response("FAIL", response, "variable '%s' not found", var);
free_oem_partitions:
	free(oem_partition_table.partitions);
}
