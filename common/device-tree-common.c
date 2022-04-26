// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2019 GlobalLogic
 */

#include <device-tree-common.h>
#include <fdt_support.h>
#include <env.h>


#if defined(RCAR_DRAM_AUTO) && defined(CONFIG_R8A7795)
	extern uint32_t _arg1;	/* defined at arch/arm/cpu/armv8/start.S */
#endif	/* defined(RCAR_DRAM_AUTO) && defined(CONFIG_R8A7795) */

/*
 * This function checks flattened device tree header @dt_tbl.
 * If @DT_TABLE_MAGIC and sizeof @dt_table_entry equal to
 * to the corresponding field in the header, then returns true.
 * In another case, returns false.
 */
static bool check_fdt_header(const struct dt_table_header *dt_tbl)
{
	if (!dt_tbl)
		return false;

	if (cpu_to_fdt32(dt_tbl->magic) != DT_TABLE_MAGIC) {
		printf("ERROR: Invalid FDT table found at 0x%lx (0x%lx)\n",
			(ulong)dt_tbl, (ulong)cpu_to_fdt32(dt_tbl->magic));
		return false;
	}

	if (cpu_to_fdt32(dt_tbl->dt_entry_size) != sizeof(struct dt_table_entry)) {
		printf("ERROR: Unsupported FDT table found at 0x%lx\n", (ulong)dt_tbl);
		return false;
	}

	return true;
}

/*
 * Function loads the proper device tree from fdt at @addr.
 */
static struct fdt_header *load_dt_at_addr(struct fdt_header *addr,
					const struct dt_table_entry *dt_entry,
					const struct dt_table_header *dt_tbl)
{
	ulong dt_offset = cpu_to_fdt32(dt_entry->dt_offset);
	ulong size = cpu_to_fdt32(dt_entry->dt_size);
	return memcpy(addr, (void *)(((uchar*)dt_tbl) + dt_offset), size);
}

/*
 * Device tree ID <board id><SiP><revision>
 * Board ID:
 *   0x00 Salvator-X
 *   0x02 StarterKit Pro
 *   0x04 Salvator-XS
 *   0x05 Salvator-MS
 *   0x0A Salvator-M
 *   0x0B StarterKit Premier
 */
ulong get_current_plat_id(void)
{
	ulong cpu_type = rmobile_get_cpu_type();
	ulong cpu_rev_integer = rmobile_get_cpu_rev_integer();
	ulong cpu_rev_fraction = rmobile_get_cpu_rev_fraction();
	int ret = -1;
	u8 board_id;
#ifdef CONFIG_DM_I2C
	struct udevice *bus;
	struct udevice *dev;
#endif

	if (cpu_type == CPU_ID_R8A7796) { /* R8A7796 */
		if ((cpu_rev_integer == 2) && (cpu_rev_fraction == 0)) { /* v2.0 force to v1.1 */
			cpu_rev_integer = 1; cpu_rev_fraction = 1;
		}
	} else if (cpu_type == CPU_ID_R8A77965) { /* R8A77965 */
		/* forcing zero, because cpu type does not fit in 4 bytes */
		cpu_rev_integer = cpu_rev_fraction = 0;
	}

	/* Build ID for current platform */
	ulong plat_id = ((cpu_rev_integer << 4) & 0x000000f0) |
					 (cpu_rev_fraction      & 0x0000000f);
	if (cpu_type == CPU_ID_R8A7795) {
		plat_id |= 0x779500;
	} else if (cpu_type == CPU_ID_R8A7796) {
		plat_id |= 0x779600;
	} else if (cpu_type == CPU_ID_R8A77965) {
		plat_id |= 0x779650;
	}

	/* Read board ID from PMIC EEPROM at offset 0x70 */
 #if defined(CONFIG_DM_I2C) && defined(CONFIG_I2C_SET_DEFAULT_BUS_NUM)
	ret = uclass_get_device_by_seq(UCLASS_I2C, CONFIG_I2C_DEFAULT_BUS_NUMBER, &bus);
	if (ret) {
		printf("%s: No bus %d\n", __func__, CONFIG_I2C_DEFAULT_BUS_NUMBER);
		return ret;
	}
	ret = i2c_get_chip(bus, I2C_POWERIC_EEPROM_ADDR, 1, &dev);
	if (!ret)
		ret = dm_i2c_read(dev, 0x70, &board_id, 1);
#endif
	if (ret ) {
		printf("Error reading platform Id (%d)\n", ret);
		return ret;
	}

	if (board_id == 0xff) { /* Not programmed? */
		board_id = 0;
	}
	plat_id |= ((board_id >> 3) << 24);

	return plat_id;
}

/*
 * This function parses @bootargs and search @androidboot.dtbo_idx
 * parameter. If it is present, parse order, in which should be applied
 * overlays. The order will be stored in @overlay_order and function
 * returns the number of overlays, which defined in the appropriative parameter.
 *
 * The parameter androidboot.dtbo_idx=x,y,z reports x, y and z as the
 * zero-based indices of the Device Tree Overlays (DTOs) from the DTBO
 * partition applied (in that order) by the bootloader to the base Device Tree.
 */
static uint32_t get_overlay_order(ulong *overlay_order, uint32_t max_size)
{
	ulong overlay_idx = 0;
	uint32_t overlay_count = 0;
	const char *cmd_parameter = "androidboot.dtbo_idx";
	const char *bootargs = env_get("bootargs");
	char *iter = strstr(bootargs, cmd_parameter);

	if (!overlay_order || !max_size) {
		printf("Wrong parameters for determining right overlay order\n");
		return overlay_count;
	}

	if (!iter) {
		return overlay_count;
	}

	iter += strlen(cmd_parameter);

	if (iter[0] != '=') {
		printf("After \"androidboot.dtbo_idx\" must be \'=\' symbol\n");
		return overlay_count;
	}
	/* Skip '=' character */
	iter++;

	while (iter[0] >= '0' && iter[0] <= '9') {
		overlay_idx = ustrtoul(iter, &iter, 10);
		if (overlay_idx >= max_size) {
			printf("Invalid overlay index (%lu)\n", overlay_idx);
			break;
		}
		overlay_order[overlay_count] = overlay_idx;
		overlay_count++;
		if (!iter || iter[0] != ',' || (overlay_count >= max_size))
			break;
		iter++;
	}

	return overlay_count;
}

/*
 * Finds dt_table_entry from loaded DT info and returns a pointer
 * to it. If an entry isn't found or DT table is incorrect returns
 * NULL.
 * Parameters:
 * @info - array, which contains info about dt\dto partition
 * @size - amount of elements in @info array
 * @plat_id - current platform id
 * @index - pointer to variable where index of founded fdt will be stored
 */
static struct dt_table_entry *get_fdt_by_plat_id(struct device_tree_info *info,
							int size, ulong plat_id, int *index)
{
	int i = 0;

	for (i = 0; i < size; ++i) {
		if (cpu_to_fdt32(info[i].entry->id) == plat_id) {
			if (index)
				*index = i;
			return info[i].entry;
		}
	}

	return NULL;
}

/*
 * Function returns the maximum possible size of overlaid device tree blob.
 */
static ulong calculate_max_dt_size(struct dt_table_entry *base_dt_entry,
					struct device_tree_info *info,
					ulong *overlay_order, uint32_t overlay_num)
{
	uint32_t i = 0;
	ulong dt_num = 0;
	ulong size = cpu_to_fdt32(base_dt_entry->dt_size);

	for (i = 0; i < overlay_num; ++i) {
		dt_num = overlay_order[i];
		size += cpu_to_fdt32(info[dt_num].entry->dt_size);
	}

	return size;
}

/*
 * On the first step, the function reads from the bootargs parameter,
 * @androidboot.dtbo_idx, where defined the indexes of overlays, which
 * need to apply to base device tree. The indexes must be comma separated
 * (without whitespaces) and start from 0. If corresponding index points
 * to overlay, with different plat_id, then it will be skipped and error
 * will be shown.
 *
 * On the second stage, the function tries to apply overlays
 * in order, which written in the @androidboot.dtbo_idx parameter. In case,
 * when it meets the overlay, that can't be applied, the cycle brakes and in
 * @load_addr will be stored base device tree with all overlays, which ware
 * applied successfully. The function returns the amount of successfully
 * applied overlays.
 *
 *   Example 1: androidboot.dtbo_idx=0,3,1,2.
 *   0, 3 and 2 overlays is correct
 *   1 - invalid overlay
 *   The function applies overlays with idx 0, 3
 *   and writes an error, that it can't apply overlay with idx=1.
 *
 *   Example 2: androidboot.dtbo_idx=2,1.
 *   1 overlay - is correct
 *   2 overlay - is incorrect
 *   The function reports, that the overlay with idx=2 is invalid
 *   and doesn't do anything.
 *
 * Parameters:
 * @load_addr - address, where will be stored overlaid device tree
 * @base_dt_entry - pointer to base device tree entry from fdt (dtb partition)
 * @dt_overlays - pointer to structure @dt_overlays, which contains info about
 * dtbo partition
 * @plat_id - platform id, got from @get_current_plat_id function.
 */
static ulong apply_all_overlays(struct fdt_header *load_addr,
				struct dt_table_entry *base_dt_entry,
				struct dt_overlays *overlays,
				ulong plat_id) {
	ulong dt_num = 0;
	ulong overlay_id = 0;
	ulong overlay_count = 0;
	struct fdt_header *result_dt_buf = NULL;
	struct fdt_header *overlay_buf = NULL;
	ulong max_dt_size = 0;
	struct dt_table_entry *overlay_dt_entry = NULL;
	ulong overlay_order[overlays->size];
	uint32_t overlay_num =
		get_overlay_order(&overlay_order[0], overlays->size);

	if (!overlay_num)
		return overlay_count;

	max_dt_size = calculate_max_dt_size(base_dt_entry, overlays->info,
				&overlay_order[0], overlay_num);
	result_dt_buf = (struct fdt_header *)malloc(max_dt_size);
	if (!result_dt_buf) {
		printf("ERROR, not enough memory for applying overlays\n");
		return overlay_count;
	}

	overlay_buf = (struct fdt_header *)malloc(max_dt_size);
	if (!overlay_buf) {
		printf("ERROR, not enough memory for applying overlays\n");
		goto free_result_buf;
	}

	memmove(result_dt_buf, load_addr, fdt_totalsize(load_addr));

	while (overlay_count < overlay_num) {
		dt_num = overlay_order[overlay_count];
		if (dt_num > overlays->size) {
			printf("Error: overlay index (%lu) is invalid\n", dt_num);
			break;
		}

		overlay_id = cpu_to_fdt32(overlays->info[dt_num].entry->id);

		if (overlay_id != plat_id && overlay_id != RCAR_GENERIC_PLAT_ID) {
			printf("Overlay with idx = %lu is not for [0x%lx] platform\n",
					dt_num, plat_id);
			break;
		}

		overlay_dt_entry = overlays->info[dt_num].entry;

		load_dt_at_addr(overlay_buf, overlay_dt_entry, overlays->dt_header);
		if (fdt_open_into(result_dt_buf, result_dt_buf,
			fdt_totalsize(result_dt_buf) + fdt_totalsize(overlay_buf))) {
			printf("ERROR, due to resizing device tree\n");
			break;
		}

		if (fdt_overlay_apply_verbose(result_dt_buf, overlay_buf)) {
			printf("ERROR, due applying overlay with idx %lu\n", dt_num);
			break;
		}

		/* Success - copy overlaid dt to load addr */
		memmove(load_addr, result_dt_buf, fdt_totalsize(result_dt_buf));
		overlay_count++;
	}

	free(overlay_buf);

free_result_buf:
	free(result_dt_buf);

	return overlay_count;
}

static char *assemble_dtbo_idx_string(struct dt_overlays *overlays)
{
	const char *dtbo_str = "androidboot.dtbo_idx=";
	/*
	 * The size is equal length of base arg str (@dtbo_str) and
	 * 3 characters for every dtbo idx (2 numbers and comma).
	 */
	const size_t buf_size = overlays->size * 3 + strlen(dtbo_str);
	int i = 0;

	if (!overlays->applied_cnt)
		return NULL;

	char *newstr = malloc(buf_size);
	if (!newstr) {
		printf("Can't allocate memory\n");
		return NULL;
	}

	strcpy(newstr, dtbo_str);

	while (i < overlays->applied_cnt) {
		if (overlays->info[overlays->order_idx[i]].idx >= MAX_OVERLAYS) {
			printf("Error, the dtbo index should be less than %d\n",
				MAX_OVERLAYS);
			free(newstr);
			return NULL;
		}
		sprintf(newstr, "%s%d%s", newstr, overlays->info[overlays->order_idx[i]].idx,
			i == (overlays->applied_cnt - 1) ? "" : ",");
		++i;
	}

	return newstr;
}

static void add_dtbo_index(struct dt_overlays *overlays)
{
	int len = 0;
	char *next, *param, *value;
	char *dtbo_vals = NULL, *dtbo_param = "androidboot.dtbo_idx";
	char *bootargs = env_get("bootargs");
	/*
	 * The size is equal length of base arg str (@dtbo_param),
	 * length of @bootargs and 3 characters for every dtbo idx
	 * (2 numbers and comma).
	 */
	size_t cmdline_size =
		overlays->size * 3 + strlen(dtbo_param);

	if (!bootargs) {
		value = assemble_dtbo_idx_string(overlays);
		if (!value)
			return;

		env_set("bootargs", value);
		free(value);
		return;
	}

	len = strlen(bootargs);
	/* Increase @buf_size size depending on the @bootargs length */
	cmdline_size += len;
	char *copy_bootargs = malloc(len + 1);
	if (!copy_bootargs) {
		printf("Can't allocate memory\n");
		return;
	}

	char *new_bootargs = malloc(cmdline_size);
	if (!new_bootargs) {
		printf("Can't allocate memory\n");
		goto add_dtbo_index_exit1;
	}

	strcpy(copy_bootargs, bootargs);
	new_bootargs[0] = '\0';
	next = skip_spaces(copy_bootargs);
	while (*next) {
		next = next_arg(next, &param, &value);
		if (param) {
			if (strcmp(dtbo_param, param) == 0) {
				dtbo_vals = value;
			} else {
				if (value) {
					if (strchr(value, ' '))
						sprintf(new_bootargs, "%s%s=\"%s\" ", new_bootargs, param, value);
					else
						sprintf(new_bootargs, "%s%s=%s ", new_bootargs, param, value);
				} else {
					sprintf(new_bootargs, "%s%s ", new_bootargs, param);
				}
			}
		}
	}

	if (!dtbo_vals) {
		dtbo_vals = assemble_dtbo_idx_string(overlays);
		if (!dtbo_vals) {
			goto add_dtbo_index_exit2;
		}
		strcat(new_bootargs, dtbo_vals);
		env_set("bootargs", new_bootargs);
		free(dtbo_vals);
	} else {
		value = assemble_dtbo_idx_string(overlays);
		if (!value) {
			goto add_dtbo_index_exit2;
		}
		sprintf(value, "%s,%s", value, dtbo_vals);
		strcat(new_bootargs, value);
		free(value);
	}

add_dtbo_index_exit2:
	free(new_bootargs);
add_dtbo_index_exit1:
	free(copy_bootargs);
	return;
}

void free_dt_info(struct device_tree_info *info, int num)
{
	int i = 0;

	if (!info)
		return;

	for (i = 0; i < num; ++i)
		free(info[i].name);

	free(info);
}
/*
 * Returns index of overlay with given @name.
 * If overlay with this @name is not found, returns
 * negative value.
 */
static int find_overlay_by_name(const struct dt_overlays *overlays,
	const char *name)
{
	bool is_found = false;
	int i = 0;

	for (i = 0; i < overlays->size; ++i) {
		if (!strncmp(name, overlays->info[i].name, strlen(overlays->info[i].name))) {
			is_found = true;
			break;
		}
	}

	if (!is_found)
		i = -1;

	return i;
}

/*
 * Parse @dtbo_names variable and detects which overlays
 * should be applied.
 */
static void add_user_overlays(struct dt_overlays *overlays)
{
	int idx = 0;
	const char *env_var = "dtbo_names";
	char *env_val = env_get(env_var);
	char *pchr = NULL;
	char *env_val_dup = NULL;
	const int custom_fields_len = 4;
	/* 3 separators between 4 custom uint32_t fields */
	const int max_name_len = custom_fields_len * sizeof(int) + 3;

	if (!env_val)
		return;

	env_val_dup = strdup(env_val);
	if (!env_val_dup) {
		printf("Error - not enough memory for duplicate environment value\n");
		return;
	}

	pchr = strtok(env_val_dup, " ,");
	do {
		if (strlen(pchr) > max_name_len) {
			printf("Error - overlay name length should be less than %d chars\n", max_name_len);
			pchr = strtok(NULL, " ,");
			continue;
		}

		idx = find_overlay_by_name(overlays, pchr);

		if (idx < 0) {
			printf("Overlay %s is not found\n", pchr);
		} else {
			overlays->order_idx[overlays->applied_cnt] = overlays->info[idx].idx;
			overlays->applied_cnt++;
		}

		pchr = strtok(NULL, " ,");
	} while (pchr);

	free(env_val_dup);
}

static void add_default_overlays(struct dt_overlays *overlays)
{
	int i = 0;
	int idx = 0;
	int default_dtbo_count = 0;
	const char *default_dtbo[MAX_DEFAULT_OVERLAYS];
	 __attribute__((unused)) uint32_t plat_id = get_current_plat_id();

#if defined(CONFIG_R8A7795)
#if defined(RCAR_DRAM_AUTO)
	if (_arg1 == 8) {
		if (plat_id == H3v3_PLAT_ID) {
			default_dtbo[0] = V3_PLATID_DTBO_NAME;
			default_dtbo[1] = V3_INFO_DTBO_NAME;
			default_dtbo_count = 2;
		} else if (plat_id == H3v2_PLAT_ID) {
			default_dtbo[0] = V2_PLATID_DTBO_NAME;
			default_dtbo[1] = V2_INFO_DTBO_NAME;
			default_dtbo_count = 2;
		}
	}
#elif defined(RCAR_DRAM_MAP4_2)
	if (plat_id == H3v3_PLAT_ID) {
		default_dtbo[0] = V3_PLATID_DTBO_NAME;
		default_dtbo[1] = V3_INFO_DTBO_NAME;
		default_dtbo_count = 2;
	} else if (plat_id == H3v2_PLAT_ID) {
		default_dtbo[0] = V2_PLATID_DTBO_NAME;
		default_dtbo[1] = V2_INFO_DTBO_NAME;
		default_dtbo_count = 2;
	}
#endif /* defined(RCAR_DRAM_AUTO) */
#endif /* defined(CONFIG_R8A7795) */

#if defined(ENABLE_ADSP)
#if defined(CONFIG_TARGET_ULCB)
	default_dtbo[default_dtbo_count] = ADSP_SKKF_DTBO_NAME;
	++default_dtbo_count;
#endif /* CONFIG_TARGET_ULCB */
#if defined(CONFIG_TARGET_SALVATOR_X)
	default_dtbo[default_dtbo_count] = ADSP_SALV_DTBO_NAME;
	++default_dtbo_count;
#endif /* CONFIG_TARGET_SALVATOR_X */
#endif /* defined(ENABLE_ADSP) */

	default_dtbo[default_dtbo_count] = AVB_DTBO_NAME;
	++default_dtbo_count;

	for (i = 0; i < default_dtbo_count; ++i) {

		idx = find_overlay_by_name(overlays, default_dtbo[i]);

		if (idx < 0) {
			printf("Overlay %s is not found\n", default_dtbo[i]);
		} else {
			overlays->order_idx[overlays->applied_cnt] = overlays->info[idx].idx;
			overlays->applied_cnt++;
		}
	}
}

/*
 * Converts 4 uint32_t custom fields from @dt_table_entry
 * structure to string. The parts are separeted via hyphen.
 * Example:
 * --custom0=0x736b6b66 --custom1=0x34783267 --custom2=0x33300000
 * will converted to string:
 * skkf-4x2g-30
 * Parameter:
 * @custom - pointer to uint32_t array with size 4
 */
static char *custom_to_str(uint32_t *custom)
{
	int j = 0;
	char *name = NULL;
	const int custom_fields_len = 4;
	const int char_per_field = sizeof(uint32_t) + 1;
	/* 3 separators between 4 custom uint32_t fields */
	const int max_name_len = custom_fields_len * sizeof(int) + 3;

	name = (char *)malloc(max_name_len + 1);
	if (!name) {
		printf("Failed to allocate memory for string \n");
		return NULL;
	}

	memset(name, 0, max_name_len + 1);

	j = 0;
	while (j < custom_fields_len && custom[j]) {
		memcpy(&name[j * char_per_field], &custom[j], sizeof(custom[j]));
		name[j * char_per_field + sizeof(custom[j])] = '-';
		j++;
	}

	if (j)
		name[(j - 1) * char_per_field + sizeof(custom[j])] = '\0';

	return name;
}

/*
 * Allocate memory for @device_tree_info struct and fills it with
 * information about device-tree based on @dt_tbl parameter.
 * The @size will chenged and will equal to amount of @dt_entry.
 * Parameters:
 * @dt_tbl - pointer to device tree table structure
 * @size - pointer to variable, where will be amount of @dt_entry
 */
struct device_tree_info *get_dt_info(struct dt_table_header *dt_tbl, int *size)
{
	int i = 0;
	ulong dt_entries_offset = 0;
	struct device_tree_info *info = NULL;
	struct dt_table_entry *dt_entry = NULL;

	if (!check_fdt_header(dt_tbl))
		return NULL;

	(*size) = cpu_to_fdt32(dt_tbl->dt_entry_count);
	dt_entries_offset = cpu_to_fdt32(dt_tbl->dt_entries_offset);
	dt_entry = (struct dt_table_entry *)(((uchar *)dt_tbl) + dt_entries_offset);

	info = (struct device_tree_info *)malloc(sizeof(*info) * (*size));
	if (!info) {
		printf("ERROR: can't allocate memory for overlay info struct\n");
		return NULL;
	}

	for (i = 0; i < (*size); ++i) {
		info[i].idx = i;
		info[i].entry = dt_entry;
		info[i].name = custom_to_str(&dt_entry->custom[0]);
		dt_entry++;
	}

	return info;
}

static struct dt_overlays *init_dt_overlays(struct device_tree_info *dt_info,
					struct dt_table_header *dt_tbl, int size)
{
	struct dt_overlays *overlays = NULL;

	if (!dt_info)
		return NULL;

	overlays = (struct dt_overlays *)malloc(sizeof(*overlays));
	if (!overlays) {
		printf("Failed to allocate memory for dt_overlays\n");
		return NULL;
	}

	overlays->size = size;
	overlays->order_idx = (int *)malloc(sizeof(int) * overlays->size);
	if (!overlays->order_idx) {
		printf("Failed to allocate memory for saving overlays order\n");
		free(overlays);
		return NULL;
	}

	overlays->info = dt_info;
	overlays->dt_header = dt_tbl;
	overlays->applied_cnt = 0;

	return overlays;
}

static void free_dt_overlays(struct dt_overlays *overlays)
{
	if (!overlays)
		return;

	if (overlays->size)
		free(overlays->order_idx);

	free_dt_info(overlays->info, overlays->size);
	free(overlays);
}

int append_partition_layout(struct fdt_header *fdt)
{
	/* Append kernel FDT with partitions layout from U-boot */
	const void *uboot_fdt = gd->fdt_blob;

	int kernel_part_node = fdt_path_offset(fdt, "/android/partitions");
	int uboot_part_node = fdt_path_offset(uboot_fdt, ANDROID_PARTITIONS_PATH);
	int uboot_subnode;
	if (kernel_part_node > 0) {
		fdt_for_each_subnode(uboot_subnode, uboot_fdt, uboot_part_node) {
			const char *name = fdt_get_name(uboot_fdt, uboot_subnode, NULL);
			int kernel_subnode = fdt_add_subnode(fdt, kernel_part_node, name);
			if (kernel_subnode < 0) {
				printf("ERROR: can not create node %s in kernel FDT\n",
					name);
				return -1;
			}

			int property;
			fdt_for_each_property_offset(property, uboot_fdt, uboot_subnode)
			{
				const char *prop_name;
				const void *prop;
				int prop_len, ret;
				prop = fdt_getprop_by_offset(uboot_fdt, property, &prop_name,
							&prop_len);
				ret = fdt_setprop(fdt, kernel_subnode, prop_name, prop, prop_len);
				if (ret) {
					printf("Failed to set %s property in %s node\n", prop_name,
						name);
					return -1;
				}
			}
		}
	} else {
		printf("ERROR: node \"/android/partition\" not found in FDT\n");
		return -1;
	}
	return 0;
}

int load_dt_with_overlays(struct fdt_header *load_addr,
				struct dt_table_header *dt_tbl,
				struct dt_table_header *dto_tbl)
{
	int dt_size = 0;
	int dto_size = 0;
	ulong applied_overlays = 0;
	ulong i = 0;
	int dtb_index = 0;
	int ret;
	struct device_tree_info *dt_info = get_dt_info(dt_tbl, &dt_size);
	struct device_tree_info *dto_info = get_dt_info(dto_tbl, &dto_size);
	struct dt_overlays *overlays = init_dt_overlays(dto_info, dto_tbl, dto_size);
	ulong plat_id = get_current_plat_id();
	struct dt_table_entry *base_dt_entry =
		get_fdt_by_plat_id(dt_info, dt_size, plat_id, &dtb_index);

	if (!base_dt_entry) {
		printf("ERROR: No proper FDT entry found for current platform id=%lx\n", plat_id);
		return -1;
	}

	/* Base device tree should be loaded in defined address */
	load_dt_at_addr(load_addr, base_dt_entry, dt_tbl);

	ret = append_partition_layout(load_addr);
	if (ret) {
		printf("Failed to append partition layout to kernel FDT\n");
		return -1;
	}

	/*
	 * Next sections is not critical - we can try to boot without overlays,
	 * so if error occurs, function should return success code.
	 */
	if (!dto_tbl) {
		printf("DTO table is NULL, load without DT overlays\n");
		return 0;
	}

	add_default_overlays(overlays);
	add_user_overlays(overlays);
	add_dtbo_index(overlays);

	applied_overlays = apply_all_overlays(load_addr, base_dt_entry,
		overlays, plat_id);

	printf("Number of dtb entries - %d\n", dt_size);
	printf("Loaded base dtb entry with id - 0x%x, index - %d\n",
			cpu_to_fdt32(base_dt_entry->id),
			dtb_index);
	printf("Applied %ld overlay(s)\n", applied_overlays);
	if(applied_overlays) {
		printf("\tIndex %16s %15s", "Name", "Board Id\n");
		for (i = 0; i < applied_overlays; ++i) {
			printf("\t  %d  -  %16s  -  0x%x\n",
				overlays->info[overlays->order_idx[i]].idx,
				overlays->info[overlays->order_idx[i]].name,
				cpu_to_fdt32(overlays->info[overlays->order_idx[i]].entry->id));
		}
	}

	free_dt_overlays(overlays);
	free_dt_info(dt_info, dt_size);

	return 0;
}

void *load_dt_table_from_part(struct blk_desc *dev_desc, const char *dtb_part_name)
{
	int ret;
	ulong size;
	ulong fdt_offset;
	void *fdt_addr;
	struct disk_partition info;

	ret = part_get_info_by_name(dev_desc, dtb_part_name, &info);
	if (ret < 0) {
		printf("ERROR: Can't read '%s' part\n", dtb_part_name);
		return NULL;
	}

	size = info.blksz * info.size;
	fdt_addr = malloc(size);
	fdt_offset = info.start;

	if (!fdt_addr) {
		printf("ERROR: Failed to allocate %lu bytes for FDT\n", size);
		return NULL;
	}

	/* Convert size to blocks */
	size /= info.blksz;

	ret = blk_dread(dev_desc, fdt_offset,
		size, fdt_addr);
	if (ret != size) {
		printf("ERROR: Can't read '%s' part\n", dtb_part_name);
		return NULL;
	}

	return fdt_addr;
}

/*
 * +---------------------+
 * | boot header         | 1 page
 * +---------------------+
 * | kernel              | n pages
 * +---------------------+
 * | ramdisk             | m pages
 * +---------------------+
 * | second stage        | o pages
 * +---------------------+
 * | recovery dtbo/acpio | p pages
 * +---------------------+
 * | dtb                 | q pages
 * +---------------------+

 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 * p = (recovery_dtbo_size + page_size - 1) / page_size
 * q = (dtb_size + page_size - 1) / page_size
 */
void *load_dt_table_from_bootimage(struct andr_img_hdr *hdr)
{
	uint32_t page_size = 0;
	uint8_t *dt_addr = (uint8_t *)hdr;

	if (!dt_addr) {
		printf("ERROR: android boot image header addr is NULL\n");
		return dt_addr;
	}

	page_size = hdr->page_size;

	/* NOTE: all addresses are aligned to page_size (2048 bytes by default)*/
	/* bootimage header 1 page */
	/* kernel_start_addr = start_addr + page_size */
	dt_addr += page_size;
	/* ramdisk_start_addr = (kernel_start_addr + kernel_size) */
	dt_addr += ALIGN(hdr->kernel_size, page_size);
	/* second_stage_bl_start = (ramdisk_start_addr + ramdisk_size) */
	dt_addr += ALIGN(hdr->ramdisk_size, page_size);
	/* recovery_dtbo_start = (second_stage_bl_start + second_stage_bl_size) */
	dt_addr += ALIGN(hdr->second_size, page_size);
	/* dtb_start = (recovery_dtbo_start + recovery_dtbo_size)*/
	dt_addr += ALIGN(hdr->recovery_dtbo_size, page_size);

	return (void *)dt_addr;
}
