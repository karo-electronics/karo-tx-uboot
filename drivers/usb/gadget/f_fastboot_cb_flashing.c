/*
 * Copyright (C) 2016 GlobalLogic
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <errno.h>
#include <mmc.h>
#include <malloc.h>
#include <fastboot.h>
#include <u-boot/crc.h>
#include <u-boot/sha256.h>
#include <avb_verify.h>
#include <mapmem.h>
#include <android/bootloader.h>
#include <env.h>


#define MMC_PERSISTENT_PART "pst"
#define SHA2_BUF_SIZE 32
#define MMC_PERSISTENT_OFFSET SHA2_BUF_SIZE
#define DRAM_BANK_MAIN	0
/*We need to define max stack size for ram wipe*/
#define MAX_STACK_SIZE 32768
#define UBOOT_RAM_SIZE 0x100000
DECLARE_GLOBAL_DATA_PTR;

#define CONFIG_FASTBOOT_CB_PART CONFIG_SYS_MMC_ENV_PART
#define CONFIG_FASTBOOT_CB_SIZE         512 /* align to flash block size */
#define CONFIG_FASTBOOT_CB_OFFSET \
		(-(CONFIG_ENV_SIZE + CONFIG_FASTBOOT_CB_SIZE))


/*Error Codes for Fastboot Control Block Functions*/

#define FB_OK				(0)
#define FB_ERR_GENERIC 	(-1)
#define FB_ERR_CRC		(-2)

int fastboot_load_control_block(struct fastboot_control_block *ctrlb)
{
	int dev = CONFIG_FASTBOOT_FLASH_MMC_DEV;
	long long offset = CONFIG_FASTBOOT_CB_OFFSET;
	size_t size = CONFIG_FASTBOOT_CB_SIZE;
	size_t blk_start, blk_count, read;
	struct mmc * mmc_dev;
	void *buffer;
	unsigned short crc, crc2;
	struct blk_desc *dev_desc;

	if ((mmc_dev = find_mmc_device(dev)) == NULL) {
		printf("MMC device not found\n");
		return FB_ERR_GENERIC;
	}
	if (mmc_switch_part(mmc_dev, CONFIG_FASTBOOT_CB_PART)) {
		printf("MMC partition switch failed\n");
		return FB_ERR_GENERIC;
	}

	dev_desc = mmc_get_blk_desc(mmc_dev);
	if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
		pr_err("invalid mmc device\n");
		return -EIO;
	}

	/* Calculate offset address and corresponded blocks to read */
	if (offset < 0)
		offset += mmc_dev->capacity;

	blk_start = ALIGN(offset, mmc_dev->read_bl_len) / mmc_dev->read_bl_len;
	blk_count = ALIGN(size, mmc_dev->read_bl_len) / mmc_dev->read_bl_len;
	/* Real size of buffer and allocate memory */
	size = (blk_count * mmc_dev->read_bl_len);
	buffer = malloc(size);

	read = blk_dread(dev_desc, blk_start, blk_count, buffer);
	memcpy(ctrlb, buffer, sizeof(struct fastboot_control_block));
	free(buffer);
	/* Switch back to last HW partition */
	mmc_switch_part(mmc_dev, MMC_DEFAULT_PARTITION);
	if (read != blk_count) {
		printf("MMC read error (count=%zu, read=%zu)\n", blk_count, read);
		return FB_ERR_GENERIC;
	}
	crc = ctrlb->crc;
	ctrlb->crc = 0;
	crc2 = crc16_ccitt(0, (uchar*)ctrlb, sizeof(struct fastboot_control_block));
	if (crc ^ crc2) {
		/* wrong CRC */
		printf ("%s: CRC Error: (0x%x, 0x%x)\n", __func__, crc, crc2);
		return FB_ERR_CRC;
	}

	return FB_OK;
}

int fastboot_save_control_block(struct fastboot_control_block *ctrlb)
{
	int dev = CONFIG_FASTBOOT_FLASH_MMC_DEV;
	long long offset = CONFIG_FASTBOOT_CB_OFFSET;
	size_t size = CONFIG_FASTBOOT_CB_SIZE;
	size_t blk_start, blk_count, wrote;
	struct mmc * mmc_dev;
	void *buffer;
	struct blk_desc *dev_desc;

	if ((mmc_dev = find_mmc_device(dev)) == NULL) {
		printf("MMC device not found\n");
		return -1;
	}
	if (mmc_switch_part(mmc_dev, CONFIG_FASTBOOT_CB_PART)) {
		printf("MMC partition switch failed\n");
		return -1;
	}

	dev_desc = mmc_get_blk_desc(mmc_dev);
	if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
		pr_err("invalid mmc device\n");
		return -EIO;
	}

	ctrlb->crc = 0;
	ctrlb->crc = crc16_ccitt(0, (uchar*)ctrlb, sizeof(struct fastboot_control_block));
	/* Calculate offset address and corresponded blocks to read */
	if (offset < 0) {
		offset += mmc_dev->capacity;
	}
	blk_start = ALIGN(offset, mmc_dev->write_bl_len) / mmc_dev->write_bl_len;
	blk_count = ALIGN(size, mmc_dev->write_bl_len) / mmc_dev->write_bl_len;
	/* Real size of buffer and allocate memory */
	size = (blk_count * mmc_dev->write_bl_len);
	buffer = malloc(size);
	memcpy(buffer, (void*)ctrlb, sizeof(struct fastboot_control_block));
	wrote = blk_dwrite(dev_desc, blk_start, blk_count, buffer);
	free(buffer);
	/* Switch back to last HW partition */
	mmc_switch_part(mmc_dev, MMC_DEFAULT_PARTITION);
	if (wrote != blk_count) {
		printf("MMC write error (count=%zu, wrote=%zu)\n", blk_count, wrote);
		return -1;
	}
	return 0;
}

int fastboot_get_lock_status(int *mmc_lock, int *ipl_lock)
{
	struct fastboot_control_block ctrlb;

	if (fastboot_load_control_block(&ctrlb)) {
		return -1;
	}

	if (mmc_lock != NULL) {
		*mmc_lock = (ctrlb.mmc_lock == FASTBOOT_MAGIC_LOCKED);
	}

	if (ipl_lock != NULL) {
		*ipl_lock = (ctrlb.ipl_lock == FASTBOOT_MAGIC_LOCKED);
	}
	return 0;
}

int fastboot_get_rollback_index(size_t location, uint64_t *idx)
{
	struct fastboot_control_block ctrlb;

	if (!idx)
		return -1;

	if (fastboot_load_control_block(&ctrlb)) {
		return -1;
	}

	/*Location out of bounds*/
	if (location >= MAX_ROLLBACK_LOCATIONS)
		return -1;

	*idx = ctrlb.rollback_cnt[location];
	return 0;
}

int fastboot_set_rollback_index(size_t location, uint64_t idx)
{
	struct fastboot_control_block ctrlb;

	if (fastboot_load_control_block(&ctrlb)) {
		return -1;
	}

	/*Location out of bounds*/
	if (location >= MAX_ROLLBACK_LOCATIONS)
		return -1;

	ctrlb.rollback_cnt[location] = idx;
	return fastboot_save_control_block(&ctrlb);
}

static inline void clear_ram_region(ulong start, ulong end)
{
	ulong size = end - start;
	void *buf = map_sysmem(start, size);

	printf("Clearing region 0x%lx .. 0x%lx...", start, end);
	memset(buf, 0, size);
	printf("done\n");
}

static void fastboot_wipe_ram(void)
{
	struct bd_info *bd = gd->bd;
	int i;
	ulong top;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; ++i) {
		if (i == DRAM_BANK_MAIN) {
			/* This is main DRAM bank where u-boot and other BLs
			 * reside. It needs special handling; First, Clear RAM area
			 * before u-boot, and then - after;
			 */
			clear_ram_region(bd->bi_dram[i].start, CONFIG_SYS_TEXT_BASE);
			top = gd->start_addr_sp - MAX_STACK_SIZE;
			clear_ram_region(CONFIG_SYS_TEXT_BASE+
						UBOOT_RAM_SIZE, top);
		} else {
			top = bd->bi_dram[i].start + bd->bi_dram[i].size;
			clear_ram_region(bd->bi_dram[i].start, top);
		}
	}
}

static int fastboot_set_wipe_data(char *response)
{
	struct bootloader_message bcb;

	if (get_bootloader_message(&bcb)) {
		fastboot_fail("load bootloader control block", response);
		return -1;
	}

	memset(&bcb.command, 0, sizeof(bcb.command));
	memset(&bcb.recovery, 0, sizeof(bcb.recovery));

	strlcpy(bcb.command, "boot-recovery", sizeof(bcb.command));

	snprintf(bcb.recovery, sizeof(bcb.recovery),
		"recovery\n--wipe_data\n--reboot_bl");

	if (set_bootloader_message(&bcb) < 0) {
		fastboot_fail("write bootloader control block", response);
		return -1;
	}

	return 0;
}

#if defined(CONFIG_FASTBOOT_BY_SW)
static long wait_pooling_to(unsigned long pooling_ms)
{
	mdelay(pooling_ms);
	return pooling_ms;
}

#define FB_USER_TO 5
#define FB_POOLING_TO 100
#define SEC_TO_MSEC(x) (1000*x)
#define MSEC_TO_SEC(x) (x/1000)

extern int gpio_get_value(unsigned gpio);
#endif /* CONFIG_FASTBOOT_BY_SW */

static bool is_user_confirmed(void)
{
	const char *confirm = NULL;
#if defined(CONFIG_FASTBOOT_BY_SW)
	long fb_delay = -1;
	char *confirm_to = NULL;
	struct confirm_pin_info *pin_info =
				gpio_get_user_confirm_pin();
#endif

	printf("WARNING: Unlocking allows flashing of unofficial images!\n");
	printf("WARNING: This  may damage your device and void warranty!\n");

	/*
	 * Instead of pressing switch, we can define confirm_user_unlock
	 * environment variable and set it to 1.
	 */
	confirm = env_get("confirm_user_unlock");
	if (confirm && !strcmp(confirm, "1")) {
		printf("user unlock confirmed by confirm_user_unlock, continue..\n");
		return true;
	}
#if defined(CONFIG_FASTBOOT_BY_SW)
	if (pin_info)
		printf("WARNING: If you want to continue, press %s!\n",
				pin_info->name);
	else
		printf("ERROR: failed to get user confirm pin information\n");

	/*User can change default TO by setting confirm_user_to variable*/
	confirm_to = env_get("confirm_user_to");
	if (confirm_to)
		fb_delay = SEC_TO_MSEC(simple_strtol(confirm_to, NULL, 10));

	if (fb_delay < 0)
		fb_delay = SEC_TO_MSEC(FB_USER_TO);

	while (pin_info && fb_delay > 0) {
		if (!gpio_get_value(pin_info->pin_num)) {
			printf("Unlocking confirmed by user, continue\n");
			return true;
		}
		fb_delay -= wait_pooling_to(FB_POOLING_TO);
		printf("\b\b\b%2d ", (int) MSEC_TO_SEC(fb_delay));
	}
#endif
	return false;
}

bool fastboot_get_unlock_ability(void)
{
	int ret;
	bool unlock = false;
	static sha256_context ctx256;
	u8 zero_buf_32[SHA2_BUF_SIZE];
	u8 sha_buf[SHA2_BUF_SIZE];
	size_t num_read = 0;
	u8 *buf = (u8 *)CONFIG_FASTBOOT_BUF_ADDR;
	uint64_t part_size = get_part_size(CONFIG_FASTBOOT_FLASH_MMC_DEV,
						MMC_PERSISTENT_PART);

	memset(zero_buf_32, 0, sizeof(zero_buf_32));

	ret = read_from_part(CONFIG_FASTBOOT_FLASH_MMC_DEV,
					MMC_PERSISTENT_PART,
					MMC_PERSISTENT_OFFSET,
					part_size - MMC_PERSISTENT_OFFSET,
					buf, &num_read);

	if (!ret && num_read) {
		unlock = (bool) buf[num_read - 1];
		printf("Unlock ability: %d\n", unlock);
		if (unlock) {
			/*Check hash for sanity*/
			sha256_starts(&ctx256);
			sha256_update(&ctx256, zero_buf_32, SHA2_BUF_SIZE);
			sha256_update(&ctx256, buf, num_read);
			sha256_finish(&ctx256, sha_buf);
			ret = read_from_part(CONFIG_FASTBOOT_FLASH_MMC_DEV,
							   MMC_PERSISTENT_PART,
							   0,
							   SHA2_BUF_SIZE,
							   zero_buf_32, &num_read);
			if (!ret && (num_read == SHA2_BUF_SIZE)) {
				ret = memcmp(zero_buf_32, sha_buf, SHA2_BUF_SIZE);
				if (ret) {
					printf("Partition hash mismatch, skipping unlock\n");
					printf("calculated hash:\n");
					for (int i = 0; i < SHA2_BUF_SIZE; i++)
						printf("%x", sha_buf[i]);

					printf("\r\n");
					printf("original hash:");
					for (int i = 0; i < SHA2_BUF_SIZE; i++)
						printf("%x", zero_buf_32[i]);

					printf("\r\n");
					unlock = false;
				}
			} else {
				printf("Hash read error, skipping unlock\n");
				unlock = false;
			}
		}
	}

	return unlock;
}

void fastboot_cb_flashing(char *cmd, char *response)
{
	struct fastboot_control_block ctrlb;
	int ret, i;
	bool wipe_ram = false;

	strsep(&cmd, " ");
	ret = fastboot_load_control_block(&ctrlb);
	if (ret == FB_ERR_GENERIC) {
		fastboot_fail("load control block failed", response);
		return;
	}
	if (ret == FB_ERR_CRC) {
		ctrlb.ipl_lock = FASTBOOT_MAGIC_UNLOCKED;
		ctrlb.mmc_lock = FASTBOOT_MAGIC_UNLOCKED;
		for (i = 0; i < ARRAY_SIZE(ctrlb.rollback_cnt); i++) {
			ctrlb.rollback_cnt[i] = 0;
		}
	}

	if (!strcmp(cmd, "lock")) {
		ctrlb.mmc_lock = FASTBOOT_MAGIC_LOCKED;
	} else if (!strcmp(cmd, "unlock")) {
		if (!fastboot_get_unlock_ability()) {
			fastboot_fail("unlock ability is 0", response);
			return;
		}
		if (!is_user_confirmed()) {
			fastboot_fail("user abort", response);
			return;
		}
		ctrlb.mmc_lock = FASTBOOT_MAGIC_UNLOCKED;
		wipe_ram = true;
	} else if (!strcmp(cmd, "lock_critical")) {
		ctrlb.ipl_lock = FASTBOOT_MAGIC_LOCKED;
	} else if (!strcmp(cmd, "unlock_critical")) {
		if (!fastboot_get_unlock_ability()) {
			fastboot_fail("unlock ability is 0", response);
			return;
		}
		if (!is_user_confirmed()) {
			fastboot_fail("user abort", response);
			return;
		}
		ctrlb.ipl_lock = FASTBOOT_MAGIC_UNLOCKED;
		wipe_ram = true;
	} else if (!strcmp(cmd, "get_unlock_ability")) {
		bool unlock = fastboot_get_unlock_ability();

		fastboot_send_response("INFO unlock ability: %d", unlock);
		fastboot_okay(NULL, response);
		return;
	} else {
		fastboot_fail("not implemented", response);
		return;
	}

	if (fastboot_save_control_block(&ctrlb)) {
		fastboot_fail("save control block failed", response);
		return;
	}
	/*We're wiping the data on lock/unlock*/
	fastboot_set_wipe_data(response);
	/*We're wiping the RAM on unlock or unlock_crytical*/
	if (wipe_ram)
		fastboot_wipe_ram();

	fastboot_set_reset_completion();
	fastboot_okay(NULL, response);
}

