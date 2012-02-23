/*
 * (C) Copyright 2003
 * Kyle Harris, kharris@nexus-tech.net
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <mmc.h>

static int curr_device = -1;
#ifndef CONFIG_GENERIC_MMC
int do_mmc (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int dev;

	if (argc < 2)
		return cmd_usage(cmdtp);

	if (strcmp(argv[1], "init") == 0) {
		if (argc == 2) {
			if (curr_device < 0)
				dev = 1;
			else
				dev = curr_device;
		} else if (argc == 3) {
			dev = (int)simple_strtoul(argv[2], NULL, 10);
		} else {
			return cmd_usage(cmdtp);
		}

		if (mmc_legacy_init(dev) != 0) {
			puts("No MMC card found\n");
			return 1;
		}

		curr_device = dev;
		printf("mmc%d is available\n", curr_device);
	} else if (strcmp(argv[1], "device") == 0) {
		if (argc == 2) {
			if (curr_device < 0) {
				puts("No MMC device available\n");
				return 1;
			}
		} else if (argc == 3) {
			dev = (int)simple_strtoul(argv[2], NULL, 10);

#ifdef CONFIG_SYS_MMC_SET_DEV
			if (mmc_set_dev(dev) != 0)
				return 1;
#endif
			curr_device = dev;
		} else {
			return cmd_usage(cmdtp);
		}

		printf("mmc%d is current device\n", curr_device);
	} else {
		return cmd_usage(cmdtp);
	}

	return 0;
}

U_BOOT_CMD(
	mmc, 3, 1, do_mmc,
	"MMC sub-system",
	"init [dev] - init MMC sub system\n"
	"mmc device [dev] - show or set current device"
);
#else /* !CONFIG_GENERIC_MMC */

<<<<<<< HEAD
#ifdef CONFIG_BOOT_PARTITION_ACCESS
#define MMC_PARTITION_SWITCH(mmc, part, enable_boot) \
	do { \
		if (IS_SD(mmc)) {	\
			if (part > 1)	{\
				printf( \
				"\nError: SD partition can only be 0 or 1\n");\
				return 1;	\
			}	\
			if (sd_switch_partition(mmc, part) < 0) {	\
				if (part > 0) { \
					printf("\nError: Unable to switch SD "\
					"partition\n");\
					return 1;	\
				}	\
			}	\
		} else {	\
			if (mmc_switch_partition(mmc, part, enable_boot) \
				< 0) {	\
				printf("Error: Fail to switch "	\
					"partition to %d\n", part);	\
				return 1;	\
			}	\
		} \
	} while (0)
#endif

=======
enum mmc_state {
	MMC_INVALID,
	MMC_READ,
	MMC_WRITE,
	MMC_ERASE,
};
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706
static void print_mmcinfo(struct mmc *mmc)
{
	printf("Device: %s\n", mmc->name);
	printf("Manufacturer ID: %x\n", mmc->cid[0] >> 24);
	printf("OEM: %x\n", (mmc->cid[0] >> 8) & 0xffff);
	printf("Name: %c%c%c%c%c \n", mmc->cid[0] & 0xff,
			(mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
			(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff);

	printf("Tran Speed: %d\n", mmc->tran_speed);
	printf("Rd Block Len: %d\n", mmc->read_bl_len);

	printf("%s version %d.%d\n", IS_SD(mmc) ? "SD" : "MMC",
			(mmc->version >> 4) & 0xf, mmc->version & 0xf);

	printf("High Capacity: %s\n", mmc->high_capacity ? "Yes" : "No");
	puts("Capacity: ");
	print_size(mmc->capacity, "\n");

	printf("Bus Width: %d-bit\n", mmc->bus_width);
}

int do_mmcinfo (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct mmc *mmc;

	if (curr_device < 0) {
		if (get_mmc_num() > 0)
			curr_device = 0;
		else {
			puts("No MMC device available\n");
			return 1;
		}
	}

	mmc = find_mmc_device(curr_device);

	if (mmc) {
<<<<<<< HEAD
		if (mmc_init(mmc))
			puts("MMC card init failed!\n");
		else
			print_mmcinfo(mmc);
=======
		mmc_init(mmc);

		print_mmcinfo(mmc);
		return 0;
	} else {
		printf("no mmc device at slot %x\n", curr_device);
		return 1;
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706
	}
}

<<<<<<< HEAD
U_BOOT_CMD(mmcinfo, 2, 0, do_mmcinfo,
	"mmcinfo <dev num>-- display MMC info",
=======
U_BOOT_CMD(
	mmcinfo, 1, 0, do_mmcinfo,
	"display MMC info",
	"    - device number of the device to dislay info of\n"
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706
	""
);

int do_mmcops(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
<<<<<<< HEAD
	int rc = 0;
#ifdef CONFIG_BOOT_PARTITION_ACCESS
	u32 part = 0;
#endif
=======
	enum mmc_state state;
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706

	if (argc < 2)
		return cmd_usage(cmdtp);

	if (curr_device < 0) {
		if (get_mmc_num() > 0)
			curr_device = 0;
		else {
			puts("No MMC device available\n");
			return 1;
		}
	}

	if (strcmp(argv[1], "rescan") == 0) {
		struct mmc *mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}

		mmc->has_init = 0;

		if (mmc_init(mmc))
			return 1;
		else
			return 0;
	} else if (strncmp(argv[1], "part", 4) == 0) {
		block_dev_desc_t *mmc_dev;
		struct mmc *mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}
		mmc_init(mmc);
		mmc_dev = mmc_get_dev(curr_device);
		if (mmc_dev != NULL &&
				mmc_dev->type != DEV_TYPE_UNKNOWN) {
			print_part(mmc_dev);
			return 0;
		}

		puts("get mmc type error!\n");
		return 1;
<<<<<<< HEAD
#ifdef CONFIG_BOOT_PARTITION_ACCESS
	case 7: /* Fall through */
		part = simple_strtoul(argv[6], NULL, 10);
#endif
	default: /* at least 5 args */
		if (strcmp(argv[1], "read") == 0) {
			int dev = simple_strtoul(argv[2], NULL, 10);
			void *addr = (void *)simple_strtoul(argv[3], NULL, 16);
			u32 cnt = simple_strtoul(argv[5], NULL, 16);
			u32 n;
			u32 blk = simple_strtoul(argv[4], NULL, 16);

			struct mmc *mmc = find_mmc_device(dev);

			if (!mmc)
=======
	} else if (strcmp(argv[1], "list") == 0) {
		print_mmc_devices('\n');
		return 0;
	} else if (strcmp(argv[1], "dev") == 0) {
		int dev, part = -1;
		struct mmc *mmc;

		if (argc == 2)
			dev = curr_device;
		else if (argc == 3)
			dev = simple_strtoul(argv[2], NULL, 10);
		else if (argc == 4) {
			dev = (int)simple_strtoul(argv[2], NULL, 10);
			part = (int)simple_strtoul(argv[3], NULL, 10);
			if (part > PART_ACCESS_MASK) {
				printf("#part_num shouldn't be larger"
					" than %d\n", PART_ACCESS_MASK);
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706
				return 1;
			}
		} else
			return cmd_usage(cmdtp);

<<<<<<< HEAD
#ifdef CONFIG_BOOT_PARTITION_ACCESS
			printf("\nMMC read: dev # %d, block # %d, "
				"count %d partition # %d ... \n",
				dev, blk, cnt, part);
#else
			printf("\nMMC read: dev # %d, block # %d,"
				"count %d ... \n", dev, blk, cnt);
#endif

			mmc_init(mmc);

#ifdef CONFIG_BOOT_PARTITION_ACCESS
			if (((mmc->boot_config &
				EXT_CSD_BOOT_PARTITION_ACCESS_MASK) != part)
				|| IS_SD(mmc)) {
				/*
				 * After mmc_init, we now know whether
				 * this is a eSD/eMMC which support boot
				 * partition
				 */
				MMC_PARTITION_SWITCH(mmc, part, 0);
			}
#endif

			n = mmc->block_dev.block_read(dev, blk, cnt, addr);
=======
		mmc = find_mmc_device(dev);
		if (!mmc) {
			printf("no mmc device at slot %x\n", dev);
			return 1;
		}
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706

		mmc_init(mmc);
		if (part != -1) {
			int ret;
			if (mmc->part_config == MMCPART_NOAVAILABLE) {
				printf("Card doesn't support part_switch\n");
				return 1;
			}

<<<<<<< HEAD
			printf("%d blocks read: %s\n",
				n, (n==cnt) ? "OK" : "ERROR");
			return (n == cnt) ? 0 : 1;
		} else if (strcmp(argv[1], "write") == 0) {
			int dev = simple_strtoul(argv[2], NULL, 10);
			void *addr = (void *)simple_strtoul(argv[3], NULL, 16);
			u32 cnt = simple_strtoul(argv[5], NULL, 16);
			u32 n;

			struct mmc *mmc = find_mmc_device(dev);
=======
			if (part != mmc->part_num) {
				ret = mmc_switch_part(dev, part);
				if (!ret)
					mmc->part_num = part;
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706

				printf("switch to partions #%d, %s\n",
						part, (!ret) ? "OK" : "ERROR");
			}
		}
		curr_device = dev;
		if (mmc->part_config == MMCPART_NOAVAILABLE)
			printf("mmc%d is current device\n", curr_device);
		else
			printf("mmc%d(part %d) is current device\n",
				curr_device, mmc->part_num);

		return 0;
	}

<<<<<<< HEAD
#ifdef CONFIG_BOOT_PARTITION_ACCESS
			printf("\nMMC write: dev # %d, block # %d, "
				"count %d, partition # %d ... \n",
				dev, blk, cnt, part);
#else
			printf("\nMMC write: dev # %d, block # %d, "
				"count %d ... \n",
				dev, blk, cnt);
#endif
=======
	if (strcmp(argv[1], "read") == 0)
		state = MMC_READ;
	else if (strcmp(argv[1], "write") == 0)
		state = MMC_WRITE;
	else if (strcmp(argv[1], "erase") == 0)
		state = MMC_ERASE;
	else
		state = MMC_INVALID;

	if (state != MMC_INVALID) {
		struct mmc *mmc = find_mmc_device(curr_device);
		int idx = 2;
		u32 blk, cnt, n;
		void *addr;

		if (state != MMC_ERASE) {
			addr = (void *)simple_strtoul(argv[idx], NULL, 16);
			++idx;
		} else
			addr = 0;
		blk = simple_strtoul(argv[idx], NULL, 16);
		cnt = simple_strtoul(argv[idx + 1], NULL, 16);

		if (!mmc) {
			printf("no mmc device at slot %x\n", curr_device);
			return 1;
		}
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706

		printf("\nMMC %s: dev # %d, block # %d, count %d ... ",
				argv[1], curr_device, blk, cnt);

<<<<<<< HEAD
#ifdef CONFIG_BOOT_PARTITION_ACCESS
			if (((mmc->boot_config &
				EXT_CSD_BOOT_PARTITION_ACCESS_MASK) != part)
				|| IS_SD(mmc)) {
				/*
				 * After mmc_init, we now know whether this is a
				 * eSD/eMMC which support boot partition
				 */
				MMC_PARTITION_SWITCH(mmc, part, 1);
			}
#endif

			n = mmc->block_dev.block_write(dev, blk, cnt, addr);
=======
		mmc_init(mmc);
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706

		switch (state) {
		case MMC_READ:
			n = mmc->block_dev.block_read(curr_device, blk,
						      cnt, addr);
			/* flush cache after read */
			flush_cache((ulong)addr, cnt * 512); /* FIXME */
			break;
		case MMC_WRITE:
			n = mmc->block_dev.block_write(curr_device, blk,
						      cnt, addr);
			break;
		case MMC_ERASE:
			n = mmc->block_dev.block_erase(curr_device, blk, cnt);
			break;
		default:
			BUG();
		}

		printf("%d blocks %s: %s\n",
				n, argv[1], (n == cnt) ? "OK" : "ERROR");
		return (n == cnt) ? 0 : 1;
	}

	return cmd_usage(cmdtp);
}

U_BOOT_CMD(
	mmc, 6, 1, do_mmcops,
	"MMC sub system",
<<<<<<< HEAD
	"mmc read <device num> addr blk# cnt\n"
	"mmc write <device num> addr blk# cnt\n"
	"mmc rescan <device num>\n"
=======
	"read addr blk# cnt\n"
	"mmc write addr blk# cnt\n"
	"mmc erase blk# cnt\n"
	"mmc rescan\n"
	"mmc part - lists available partition on current mmc device\n"
	"mmc dev [dev] [part] - show or set current mmc device [partition]\n"
>>>>>>> 9a3aae22edf1eda6326cc51c28631ca5c23b7706
	"mmc list - lists available devices");
#else
U_BOOT_CMD(
	mmc, 7, 1, do_mmcops,
	"MMC sub system",
	"mmc read <device num> addr blk# cnt [partition]\n"
	"mmc write <device num> addr blk# cnt [partition]\n"
	"mmc rescan <device num>\n"
	"mmc list - lists available devices");
#endif
