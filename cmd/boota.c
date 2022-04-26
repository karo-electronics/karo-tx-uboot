// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <bootm.h>
#include <command.h>
#include <image.h>
#include <lmb.h>
#include <mapmem.h>
#include <linux/kernel.h>
#include <linux/sizes.h>
#include <mmc.h>
#include <u-boot/zlib.h>
#include <android_image.h>
#include <avb_verify.h>
#include <dt_table.h>
#include <malloc.h>
#include <configs/rcar-gen3-common.h>
#include <device-tree-common.h>
#include <android/bootloader.h>
#include <u-boot/crc.h>
#include <u-boot/md5.h>
#include <gzip.h>
#include <lz4.h>
#include <linux/delay.h>

/*
 * Android Image booting support on R-Car Gen3 boards
 */

/* Unpacked kernel image size must be no more than
 * andr_img_hdr.ramdisk_addr -  andr_img_hdr.kernel_addr,
 * else data will be partially erased
 */
#define KERNEL_UNPACKED_LIMIT      0x1080000UL

/*We will use this for zipped kernel loading*/
#define GZIP_LOAD_ADDR	0x4c000000
#define GZIP_MAGIC 0x8B1F
static inline bool is_zipped(ulong addr)
{
	u16 *magic = (u16 *) addr;
	if (cpu_to_le16(*magic) == GZIP_MAGIC)
		return true;

	return false;

}

/*
 * platform_get_random - Get device specific random number
 * @rand:		random number buffer to be filled
 * @bytes:		Number of bytes of random number to be supported
 * @eret:		-1 in case of error, 0 for success
 */
static int platform_get_random(uint8_t *rand, int bytes)
{
	union {
		uint64_t ticks;
		unsigned char buf[8];
	}	entropy;
	unsigned char md5_out[16];

	if (!bytes || bytes > 8) {
		printf("Error: max random bytes genration supported is 8\n");
		return -1;
	}

	entropy.ticks = get_ticks();
	md5 (entropy.buf, 8, md5_out);
	memcpy(rand, md5_out, bytes);

	return 0;
}

/*
 * fdt_fix_kaslr - Add kalsr-seed node in Device tree
 * @fdt:		Device tree
 * @eret:		0 in case of error, 1 for success
 */
static int fdt_fixup_kaslr(void *fdt)
{
	int nodeoffset;
	int err, ret = 0;
	u8 rand[8];

	ret = platform_get_random(rand, 8);
	if (ret < 0) {
		printf("WARNING: No random number to set kaslr-seed\n");
		return ret;
	}

	err = fdt_check_header(fdt);
	if (err < 0) {
		printf("fdt_chosen: %s\n", fdt_strerror(err));
		return ret;
	}

	/* find or create "/chosen" node. */
	nodeoffset = fdt_find_or_add_subnode(fdt, 0, "chosen");
	if (nodeoffset < 0)
		return ret;

	err = fdt_setprop(fdt, nodeoffset, "kaslr-seed", rand,
				  sizeof(rand));
	if (err < 0) {
		printf("WARNING: can't set kaslr-seed %s.\n",
		       fdt_strerror(err));
		return ret;
	}
	ret = 1;

	return ret;
}

#ifdef CONFIG_LZ4
#define LZ4_MAGIC 0x2204
static inline bool is_lz4(ulong addr)
{
	u16 *magic = (u16 *) addr;
	if (cpu_to_le16(*magic) == LZ4_MAGIC)
		return true;

	return false;

}
#endif

static void set_board_id_args(ulong plat_id)
{
	char *bootargs = env_get("bootargs");
	int len = 0;
	if (bootargs)
		len += strlen(bootargs);
	len += 34;	/* for 'androidboot.board_id=0xXXXXXXXX '*/
	char *newbootargs = malloc(len);
	if (newbootargs) {
		snprintf(newbootargs, len, "androidboot.board_id=0x%lx %s",
			plat_id, bootargs);
		env_set("bootargs", newbootargs);
	} else {
		puts("Error: malloc in set_board_id_args failed!\n");
		return;
	}

	free(newbootargs);
}

static void __maybe_unused set_cpu_revision_args(void)
{
	char *bootargs = env_get("bootargs");
	int len = 0;
	u32 rev_integer = rmobile_get_cpu_rev_integer();
	u32 rev_fraction = rmobile_get_cpu_rev_fraction();
	u32 cpu_type = rmobile_get_cpu_type();

	if (cpu_type == CPU_ID_R8A7796) { /* R8A7796 */
		if ((rev_integer == 2) && (rev_fraction == 0)) {
			/* v2.0 force to v1.1 */
			rev_integer = rev_fraction = 1;
		}
	}

	if (bootargs)
		len += strlen(bootargs);
	len += 27; /* for 'androidboot.revision=x.x '*/

	char *newbootargs = malloc(len);
	if (newbootargs) {
		snprintf(newbootargs, len, "androidboot.revision=%d.%d %s",
				rev_integer, rev_fraction, bootargs);
		env_set("bootargs", newbootargs);
	} else {
		puts("Error: malloc in set_cpu_revision_args failed!\n");
		return;
	}

	free(newbootargs);
}

#define OTA_CRITICAL_PART "blkdevparts=mmcblk0boot0:%u(bootloader_a);" \
		"mmcblk0boot1:%u(bootloader_b)"
static void set_blkdevparts_args(void)
{
	char *bootargs = env_get("bootargs");
	int len = 0;
	int ipl_locked = 0;
	char buf[128];
	uint32_t bl_size = get_bootloader_size();
	if (bootargs)
		len += strlen(bootargs);
	if (!fastboot_get_lock_status(NULL, &ipl_locked)) {
		if (!ipl_locked) {
			sprintf(buf, OTA_CRITICAL_PART, bl_size, bl_size);
			len += strlen(buf) + 2;
			char *newbootargs = malloc(len);
			if (newbootargs) {
				snprintf(newbootargs, len, "%s %s", buf, bootargs);
				env_set("bootargs", newbootargs);
			} else {
				puts("Error: malloc in set_blkdevparts_args failed!\n");
				return;
			}
			free(newbootargs);
		}
	}
}

static void set_fakertc_args(void)
{
	char *bootargs = env_get("bootargs");
	int len = 0;
	if (bootargs)
		len += strlen(bootargs);
	len += 21;	/* for 'init_time=1563524000 '*/
	char *newbootargs = malloc(len);
	if (newbootargs) {
		snprintf(newbootargs, len, "init_time=%u %s",
				(unsigned int)env_get_ulong("rtc_time", 10, RTC_TIME_SEC),
				bootargs);
		env_set("bootargs", newbootargs);
	} else {
		puts("Error: malloc in set_fakertc_args failed!\n");
		return;
	}
	free(newbootargs);
}

/* Function adds 'androidboot.bootreason=xxxxx' to bootargs */
static void set_bootreason_args(u32 addr) {
	char *bootargs = env_get("bootargs");
	int lenargs = 0;
	char *bootreason_mem_buf = NULL;
	const size_t REASON_MIN_LEN = 2;
	const size_t REASON_MAX_LEN = 128;
	bool bootreason_en = false;
	char bootreason_args[REASON_MAX_LEN];

	if (bootargs)
		lenargs += strlen(bootargs);

	/* First try to read from RAM (written by bootreason driver)
	 * Try to parse rambootreason RAM address from DT
	 */
	struct fdt_header *fdt = map_sysmem(addr, 0);
	int nodeoffset = fdt_path_offset(fdt,
			"/reserved-memory/rambootreason");

	if (nodeoffset > 0) {
		fdt_addr_t addr = fdtdec_get_addr(fdt, nodeoffset, "reg");
		if (addr != FDT_ADDR_T_NONE) {
			bootreason_mem_buf =
					(char *)map_sysmem(addr, 0);
		}
	}

	if (bootreason_mem_buf) {
		/* Got rambootreason buffer */
		struct bootreason_message msg;
		/* Copy bootreason struct */
		memcpy(&msg, bootreason_mem_buf,
				sizeof(struct bootreason_message));
		/* Check crc32 */
		if (crc32(0, (const unsigned char *)msg.reason,
				sizeof(msg.reason)) == msg.crc) {
			if (strlen(msg.reason) > REASON_MIN_LEN &&
					strlen(msg.reason) < REASON_MAX_LEN) {
				lenargs += REASON_MAX_LEN;
				bootreason_en = true;
				memset(bootreason_args, 0, sizeof(bootreason_args));
				/* Add bootreason value */
				snprintf(bootreason_args, REASON_MAX_LEN,
						"androidboot.bootreason=%s", msg.reason);
			}
		}
		/* Clear memory */
		memset(bootreason_mem_buf, 0, sizeof(struct bootreason_message));
	}

	if (!bootreason_en) {
		/* Then try to read from BCB (written by bcb driver) */
		struct bootloader_message bcb;

		if (get_bootloader_message(&bcb) == 0) {
			/* Got BCB */
			struct bootreason_message msg;
			/* Copy bootreason struct */
			memcpy(&msg, bcb.reserved, sizeof(struct bootreason_message));
			/* Check crc32 */
			if (crc32(0, (const unsigned char *)msg.reason,
					sizeof(msg.reason)) == msg.crc) {
				if (strlen(msg.reason) > REASON_MIN_LEN &&
						strlen(msg.reason) < REASON_MAX_LEN) {
					lenargs += REASON_MAX_LEN;
					bootreason_en = true;
					memset(bootreason_args, 0, sizeof(bootreason_args));
					/* Add bootreason value */
					snprintf(bootreason_args, REASON_MAX_LEN,
							"androidboot.bootreason=%s", msg.reason);
					/* Clear only 'reserved' part of bootloader message */
					memset(&bcb.reserved, 0, sizeof(bcb.reserved));
					set_bootloader_message(&bcb);
				}
			}
		}
	}

	/* NOTE: no need to set 'androidboot.bootreason=unknown',
	 * because Android 'bootstat' service will map this value auto
	 * if boot reason is not present in command line */
	if (!bootreason_en) {
		lenargs += REASON_MAX_LEN;
		bootreason_en = true;
		memset(bootreason_args, 0, sizeof(bootreason_args));
		/* Add bootreason value */
		snprintf(bootreason_args, REASON_MAX_LEN, "androidboot.bootreason=%s",
				"unknown");
	}

	if (bootreason_en) {
		char *newbootargs = malloc(lenargs);
		if (newbootargs) {
			printf("Bootreason: %s\n", bootreason_args);
			snprintf(newbootargs, lenargs, "%s %s", bootreason_args, bootargs);
			env_set("bootargs", newbootargs);
			free(newbootargs);
		}
	}
}

#define MMC_HEADER_SIZE 4 /*defnes header size in blocks*/
int do_boot_android_img_from_ram(ulong hdr_addr, ulong dt_addr, ulong dto_addr)
{
	ulong kernel_offset, ramdisk_offset, size;
	size_t dstn_size, kernel_space = KERNEL_UNPACKED_LIMIT;
	struct andr_img_hdr *hdr = map_sysmem(hdr_addr, 0);
	struct dt_table_header *dt_tbl = map_sysmem(dt_addr, 0);
	struct dt_table_header *dto_tbl = map_sysmem(dto_addr, 0);
	int ret;

	set_board_id_args(get_current_plat_id());
	set_cpu_revision_args();
	set_blkdevparts_args();
	set_fakertc_args();

	ret = android_image_get_kernel(hdr, 1, &kernel_offset, &size);
	if (ret) {
		printf("Error: can't extract kernel offset from "
				"boot.img (%d)\n", ret);
		return CMD_RET_FAILURE;
	}
	size = ALIGN(hdr->kernel_size, hdr->page_size);
	dstn_size = size;

	if (hdr->kernel_addr < hdr->ramdisk_addr)
		kernel_space = hdr->ramdisk_addr - hdr->kernel_addr;
	else if (hdr->kernel_addr < CONFIG_SYS_TEXT_BASE)
		kernel_space = CONFIG_SYS_TEXT_BASE - hdr->kernel_addr;

	if(is_zipped((ulong) kernel_offset)) {
		dstn_size = hdr->kernel_size;
		ret = gunzip((void *)(ulong)hdr->kernel_addr, (int)kernel_space,
						(unsigned char *)kernel_offset, (ulong*)&dstn_size);
		if (ret) {
			printf("Kernel unzip error: %d\n", ret);
			return CMD_RET_FAILURE;
		} else
			printf("Unzipped kernel image size: %zu\n", dstn_size);
	} else
#ifdef CONFIG_LZ4
	if (is_lz4((ulong) kernel_offset)) {
		dstn_size = kernel_space;
		ret = ulz4fn((void *)kernel_offset, hdr->kernel_size,
						(void *)(ulong)hdr->kernel_addr, &dstn_size);
		if (ret) {
			printf("Kernel LZ4 decompression error: %d (decompressed "
						"size: %zu bytes)\n",ret, dstn_size);
			return CMD_RET_FAILURE;
		} else
			printf("LZ4 decompressed kernel image size: %zu\n", dstn_size);
	} else
#endif
		memcpy((void *)(u64)hdr->kernel_addr,(void *)kernel_offset, dstn_size);

	printf("kernel offset = %lx, size = 0x%lx, address 0x%x\n", kernel_offset,
				dstn_size, hdr->kernel_addr);

	ramdisk_offset = kernel_offset;
	ramdisk_offset += size;
	size = ALIGN(hdr->ramdisk_size, hdr->page_size);

	printf("ramdisk offset = %lx, size = 0x%lx, address = 0x%x\n",
				ramdisk_offset, size, hdr->ramdisk_addr);

	if (hdr->ramdisk_addr != ramdisk_offset)
		memcpy((void *)(u64)hdr->ramdisk_addr, (void *)ramdisk_offset, size);


	ret = load_dt_with_overlays((struct fdt_header *)(u64)hdr->dtb_addr, dt_tbl, dto_tbl);
	fdt_fixup_kaslr((void *)(u64)hdr->dtb_addr);

	if (!ret)
		set_bootreason_args(hdr->dtb_addr);

	return ret;
}

static char hexc[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
static char *hex_to_str(u8 *data, size_t size)
{
	static char load_addr[32];
	char *paddr = &load_addr[31];
	int i;

	if (size > sizeof(load_addr) - 1)
		return NULL;

	*paddr-- =  '\0';
	for (i = 0; i < size; i++) {
		*paddr-- = (hexc[data[i] & 0x0f]);
		*paddr-- = (hexc[(data[i] >> 4) & 0x0f]);
	}
	return paddr + 1;
}

#define MAX_BOOTI_ARGC 4
static void build_new_args(ulong addr, char *argv[MAX_BOOTI_ARGC]) {
	struct andr_img_hdr *hdr = map_sysmem(addr, 0);

	argv[1] = avb_strdup(hex_to_str((u8 *)&hdr->kernel_addr, sizeof(hdr->kernel_addr)));
	argv[2] = avb_strdup(hex_to_str((u8 *)&hdr->ramdisk_addr, sizeof(hdr->ramdisk_addr)));
	argv[3] = avb_strdup(hex_to_str((u8 *)&hdr->dtb_addr, sizeof(hdr->dtb_addr)));
}

/*
 * Sets corret boot address if image was loaded
* using fastboot
*/
#define DEFAULT_RD_ADDR		0x4a180000
#define DEFAULT_SECOND_ADDR	0x48000800

/*We need virtual device and partition to support fastboot boot command*/
#define VIRT_BOOT_DEVICE	(-1)
#define VIRT_BOOT_PARTITION "RAM"
#define VIRT_SYS_LOAD_ADDR	0x48080000

void do_correct_boot_address(ulong hdr_addr)
{
	struct andr_img_hdr *hdr = map_sysmem(hdr_addr, 0);
	hdr->kernel_addr = VIRT_SYS_LOAD_ADDR;
	hdr->ramdisk_addr = DEFAULT_RD_ADDR;
	hdr->dtb_addr = DEFAULT_SECOND_ADDR;
}

static inline void avb_set_boot_device(AvbOps *ops, int boot_device)
{
	struct AvbOpsData *data = (struct AvbOpsData *) ops->user_data;
	data->mmc_dev = boot_device;
}

int inc_metadata_tries_remaining(AvbABOps* ab_ops) {
	AvbIOResult io_ret;
	AvbABData ab_data;
	int active_slot;

	io_ret = ab_ops->read_ab_metadata(ab_ops, &ab_data);
	if (io_ret != AVB_IO_RESULT_OK) {
		avb_error("I/O error while loading A/B metadata.\n");
		return io_ret;
	}

	/* Need to understand which slot is active (by their priority) */
	active_slot = (ab_data.slots[0].priority > ab_data.slots[1].priority) ?
			0 : 1;

	/*
	 * In case of at least one successful_boot tries counter value
	 * is invalid. Also we don't need to increment tries counter
	 * when it reaches 0 or is bigger or equal than max count.
	 */
	if (!ab_data.slots[active_slot].successful_boot &&
		ab_data.slots[active_slot].tries_remaining &&
		(ab_data.slots[active_slot].tries_remaining < AVB_AB_MAX_TRIES_REMAINING))
	{
		ab_data.slots[active_slot].tries_remaining++;

		/* Save metadata back to eMMC */
		io_ret = ab_ops->write_ab_metadata(ab_ops, &ab_data);
		if (io_ret != AVB_IO_RESULT_OK) {
			avb_error("I/O error while writing A/B metadata.\n");
			return io_ret;
		}
	}

	return 0;
}

#define DEFAULT_AVB_DELAY 5
int avb_main(int boot_device, char **argv)
{
	AvbOps *ops;
	AvbABFlowResult ab_result;
	AvbSlotVerifyData *slot_data;
	const char *requested_partitions[] = {"boot", "dtbo", NULL};
	bool unlocked = false;
	char *cmdline = NULL;
	bool abort = false;
	int boot_delay;
	unsigned long ts;
	const char *avb_delay  = env_get("avb_delay");
	AvbPartitionData *avb_boot_part = NULL, *avb_dtbo_part = NULL, avb_ram_data;
	AvbSlotVerifyFlags flags = AVB_SLOT_VERIFY_FLAGS_NONE;

	boot_delay = avb_delay ? (int)simple_strtol(avb_delay, NULL, 10)
						: DEFAULT_AVB_DELAY;

	avb_printv("AVB-based bootloader using libavb version ",
		avb_version_string(),
		"\n",
		NULL);

	ops = avb_ops_alloc(boot_device);
	if (!ops) {
		avb_fatal("Error allocating AvbOps.\n");
	}

	if (ops->read_is_device_unlocked(ops, &unlocked) != AVB_IO_RESULT_OK) {
		avb_fatal("Error determining whether device is unlocked.\n");
	}
	avb_printv("read_is_device_unlocked() ops returned that device is ",
		unlocked ? "UNLOCKED" : "LOCKED",
		"\n",
		NULL);

	printf("boot_device = %d\n", boot_device);

	if (boot_device == VIRT_BOOT_DEVICE) {
		/*
		* We are booting by fastboot boot command
		* This is only supported in unlocked state
		*/
		printf("setting ram partition..\n");
		if (unlocked) {
			requested_partitions[0] = "dtbo";
			requested_partitions[1] = NULL;

			avb_ram_data.partition_name = VIRT_BOOT_PARTITION;
			avb_ram_data.data = (uint8_t*)simple_strtol(argv[0], NULL, 16);

			avb_boot_part = &avb_ram_data;
			boot_device = CONFIG_FASTBOOT_FLASH_MMC_DEV;
			do_correct_boot_address((ulong) avb_ram_data.data);
		} else {
			avb_fatal("Fastboot boot not supported in locked state!\n");
			return -1;
		}
	}
	avb_set_boot_device(ops, boot_device);
	if (unlocked)
		flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;

	/*
	 * Metadata boot retry counter should be incremented after we've
	 * came here. It is needed due to the fact that BL2 decrements
	 * this counter too. Without this, in case of corruption of some
	 * Android images total retry counter will be twice smaller than
	 *  needed AVB_AB_MAX_TRIES_REMAINING.
	 */
	if (inc_metadata_tries_remaining(ops->ab_ops))
		avb_fatal("Failed to update metadata boot retry counter!\n");

	ab_result = avb_ab_flow(ops->ab_ops,
			requested_partitions,
			flags,
			AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
			&slot_data);
	avb_printv("avb_ab_flow() returned ",
		avb_ab_flow_result_to_string(ab_result),
		"\n",
		NULL);
	switch (ab_result) {
	case AVB_AB_FLOW_RESULT_OK_WITH_VERIFICATION_ERROR:
		if (!unlocked) {
			avb_fatal("Verification Error in Locked State!\n");
			break;
		}
		/*We are in unlocked state
		* Set warning and wait for user interaction;
		*/
		avb_printv("OS was not verified! Press any key to halt booting!..\n", NULL);
		while ((boot_delay > 0) && (!abort)) {
			--boot_delay;
			/* delay 1000 ms */
			ts = get_timer(0);
			do {
				if (tstc()) {	/* we got a key press	*/
					abort  = 1; /* don't auto boot	*/
					boot_delay = 0;	/* no more delay	*/
					(void) getc();	/* consume input	*/
					break;
				}
				udelay(10000);
			} while (!abort && get_timer(ts) < 1000);
			printf("\b\b\b%2d ", boot_delay);
		}
		if (abort) {
			avb_fatal("Booting halted by user request\n");
			break;
		}
		/*Fall Through*/
	case AVB_AB_FLOW_RESULT_OK:
		avb_printv("slot_suffix:    ", slot_data->ab_suffix, "\n", NULL);
		avb_printv("cmdline:        ", slot_data->cmdline, "\n", NULL);
		avb_printv(
		"release string: ",
		(const char *)((((AvbVBMetaImageHeader *)
		(slot_data->vbmeta_images[0]
		.vbmeta_data)))->release_string),
		"\n",
		NULL);
		cmdline = prepare_bootcmd_compat(ops, boot_device,
								ab_result, unlocked,
								slot_data,
								LOAD_AVB_ARGS);

		if (!cmdline) {
			avb_fatal("Error while setting cmd line\n");
			break;
		}

		if (!avb_boot_part)
			*argv = hex_to_str((u8 *)&slot_data->loaded_partitions->data, sizeof(ulong));

		AvbPartitionData *avb_part = slot_data->loaded_partitions;

		for(int i = 0; i < slot_data->num_loaded_partitions; i++, avb_part++)
		{
			if (!avb_boot_part &&
			    !strncmp(avb_part->partition_name, "boot", 4)) {
					avb_boot_part = avb_part;
			} else if (strncmp(avb_part->partition_name, "dtbo", 4) == 0) {
				avb_dtbo_part = avb_part;
			}
		}

		if (avb_boot_part == NULL || avb_dtbo_part == NULL) {
			if (avb_boot_part == NULL) {
				avb_fatal("Boot partition is not found\n");
			}
			if (avb_dtbo_part == NULL) {
				avb_fatal("dtbo partition is not found\n");
			}
		} else {
			return do_boot_android_img_from_ram((ulong)avb_boot_part->data,
							(ulong)load_dt_table_from_bootimage((void *)avb_boot_part->data),
							(ulong)avb_dtbo_part->data);
		}

	case AVB_AB_FLOW_RESULT_ERROR_OOM:
		avb_fatal("OOM error while doing A/B select flow.\n");
		break;
	case AVB_AB_FLOW_RESULT_ERROR_IO:
		avb_fatal("I/O error while doing A/B select flow.\n");
		break;
	case AVB_AB_FLOW_RESULT_ERROR_NO_BOOTABLE_SLOTS:
		avb_fatal("No bootable slots - enter repair mode\n");
		break;
	case AVB_AB_FLOW_RESULT_ERROR_INVALID_ARGUMENT:
		avb_fatal("Invalid Argument error while doing A/B select flow.\n");
		break;
	}
	avb_ops_free(ops);
	return 0;
}

int do_boot_avb(int device, char **argv)
{
	return avb_main(device, argv);
}

#define MMC_HEADER_SIZE 4 /*defnes header size in blocks*/
#define RAM_PARTITION "RAM" /*This is virtual partition when image is in RAM*/
int do_boota(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	struct mmc *mmc;
	const int user_part = 0;
	int dev = 0, ret = CMD_RET_FAILURE;
	ulong addr;
	char *boot_part = NULL;
	bool load = true;
	char *new_argv[MAX_BOOTI_ARGC];

	new_argv[0] = argv[0];
	argc--; argv++;

	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	dev = (int) simple_strtol(argv[0], NULL, 10);
	argc--; argv++;

	boot_part = argv[0];
	printf("AVB verification is ON ..\n");
	argc--; argv++;

	if (boot_part && !strncmp(boot_part, RAM_PARTITION, sizeof(RAM_PARTITION))) {
		load = false;
	}

	addr = simple_strtoul(argv[0], NULL, 16);
	if (load) {
		printf("Looking for mmc device ..\n");
		mmc = find_mmc_device(dev);
		if (!mmc)
			return CMD_RET_FAILURE;
		printf("Found (0x%p)\n", mmc);

		if (mmc_init(mmc))
			return CMD_RET_FAILURE;

		printf("Switching to partition\n");

		ret = mmc_switch_part(mmc, user_part);
		printf("switch to HW partition #%d, %s\n",
				user_part, (!ret) ? "OK" : "ERROR");
		if (ret)
			return CMD_RET_FAILURE;
	} else {
		new_argv[1] = argv[0];
	}

	ret = do_boot_avb(dev, &new_argv[1]);
	if (ret == CMD_RET_SUCCESS) {
		addr = simple_strtoul(new_argv[1], NULL, 16);
	}

	if(ret != CMD_RET_SUCCESS) {
		printf("ERROR: Boot Failed!\n");
		return ret;
	}

	build_new_args(addr, new_argv);
	argc = MAX_BOOTI_ARGC;
	images.os.start = addr;
	return do_booti(cmdtp, flag, argc, new_argv);
}

static char boota_help_text[] ="mmc_dev [mmc_part] boot_addr \n"
	"	  - boot Android Image from MMC\n"
	"\tThe argument 'mmc_dev' defines mmc device\n"
	"\tThe argument 'mmc_part' is optional and defines mmc partition\n"
	"\tdefault partiotion is 'boot' \n"
	"\tThe argument boot_addr defines memory address for booting\n"
	"";

U_BOOT_CMD(
	boota,  CONFIG_SYS_MAXARGS, 1,  do_boota,
	"boot Android Image from mmc", boota_help_text
);

