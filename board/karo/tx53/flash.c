/*
 * Copyright (C) 2011-2015 Lothar Waßmann <LW@KARO-electronics.de>
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
#include <malloc.h>
#include <nand.h>
#include <errno.h>

#include <linux/err.h>
#include <jffs2/load_kernel.h>

struct mx53_fcb {
	u32 rsrvd0;
	u32 fingerprint;
	u32 version;
	u32 rsrvd1[23];
	u32 fw1_start_page;
	u32 fw2_start_page;
	u32 rsrvd2[2];
	u32 dbbt_search_area;
	u32 bb_mark_phys_offset;
	u32 rsrvd3[11];
	u32 bb_swap;
	u32 bb_mark_byte;
	u32 rsrvd4[83];
};

static nand_info_t *mtd = &nand_info[0];
static bool doit;

static inline int calc_bb_offset(struct mx53_fcb *fcb)
{
	int ecc_block_size = 512;
	int ecc_size = mtd->oobsize / (mtd->writesize / ecc_block_size);
	int bb_mark_chunk, bb_mark_chunk_offs;

	bb_mark_chunk = mtd->writesize / (ecc_block_size + ecc_size);
	bb_mark_chunk_offs = mtd->writesize % (ecc_block_size + ecc_size);
	if (bb_mark_chunk_offs > ecc_block_size) {
		printf("Unsupported ECC layout; BB mark resides in ECC data: %u\n",
			bb_mark_chunk_offs);
		return -EINVAL;
	}
	printf("BB mark is in block %d offset %d\n",
		bb_mark_chunk, bb_mark_chunk_offs);

	return bb_mark_chunk * ecc_block_size + bb_mark_chunk_offs;
}

/*
 * return number of blocks to skip for a contiguous partition
 * of given # blocks
 */
static int find_contig_space(int block, int num_blocks, int max_blocks)
{
	int skip = 0;
	int found = 0;
	int last = block + max_blocks;

	debug("Searching %u contiguous blocks from %d..%d\n",
		num_blocks, block, block + max_blocks - 1);
	for (; block < last; block++) {
		if (nand_block_isbad(mtd, block * mtd->erasesize)) {
			skip += found + 1;
			found = 0;
			debug("Skipping %u blocks to %u\n",
				skip, block + 1);
		} else {
			found++;
			if (found >= num_blocks) {
				debug("Found %u good blocks from %d..%d\n",
					found, block - found + 1, block);
				return skip;
			}
		}
	}
	return -ENOSPC;
}

#define offset_of(p, m)		((void *)&(p)->m - (void *)(p))
#define pr_fcb_val(p, n)	debug("%-24s[%02x]=%08x(%d)\n", #n, offset_of(p, n), (p)->n, (p)->n)

static struct mx53_fcb *create_fcb(void *buf, int fw1_start_block,
				int fw2_start_block, int fw_num_blocks)
{
	struct mx53_fcb *fcb;
	u32 sectors_per_block = mtd->erasesize / mtd->writesize;

	fcb = buf;

	memset(fcb, 0x00, sizeof(*fcb));
	memset(fcb + 1, 0xff, SZ_1K - sizeof(*fcb));

	strncpy((char *)&fcb->fingerprint, "FCB ", 4);
	fcb->version = 1;

	fcb->fw1_start_page = fw1_start_block * sectors_per_block;
	pr_fcb_val(fcb, fw1_start_page);

	if (fw2_start_block != 0 &&
		fw2_start_block < lldiv(mtd->size, mtd->erasesize)) {
		fcb->fw2_start_page = fw2_start_block * sectors_per_block;
		pr_fcb_val(fcb, fw2_start_page);
	}

	return fcb;
}

static int find_fcb(void *ref, int page)
{
	int ret = 0;
	struct nand_chip *chip = mtd->priv;
	void *buf = malloc(mtd->erasesize);

	if (buf == NULL) {
		return -ENOMEM;
	}
	chip->select_chip(mtd, 0);
	chip->cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
	ret = chip->ecc.read_page_raw(mtd, chip, buf, 1, page);
	if (ret) {
		printf("Failed to read FCB from page %u: %d\n", page, ret);
		goto out;
	}
	if (memcmp(buf, ref, mtd->writesize) == 0) {
		debug("Found FCB in page %u (%08x)\n",
			page, page * mtd->writesize);
		ret = 1;
	}
out:
	chip->select_chip(mtd, -1);
	free(buf);
	return ret;
}

static int write_fcb(void *buf, int block)
{
	int ret;
	struct nand_chip *chip = mtd->priv;
	int page = block * mtd->erasesize / mtd->writesize;

	ret = find_fcb(buf, page);
	if (ret > 0) {
		printf("FCB at block %d is up to date\n", block);
		return 0;
	}

	if (doit) {
		ret = nand_erase(mtd, block * mtd->erasesize, mtd->erasesize);
		if (ret) {
			printf("Failed to erase FCB block %u\n", block);
			return ret;
		}
	}

	printf("Writing FCB to block %d @ %08llx\n", block,
		(u64)block * mtd->erasesize);
	if (doit) {
		chip->select_chip(mtd, 0);
		ret = chip->write_page(mtd, chip, 0, mtd->writesize,
				buf, 1, page, 0, 0);
		if (ret) {
			printf("Failed to write FCB to block %u: %d\n", block, ret);
		}
		chip->select_chip(mtd, -1);
	}
	return ret;
}

#define chk_overlap(a,b)				\
	((a##_start_block <= b##_end_block &&		\
		a##_end_block >= b##_start_block) ||	\
	(b##_start_block <= a##_end_block &&		\
		b##_end_block >= a##_start_block))

#define fail_if_overlap(a,b,m1,m2) do {					\
	if (!doit)							\
		printf("check: %s[%lu..%lu] <> %s[%lu..%lu]\n",		\
			m1, a##_start_block, a##_end_block,		\
			m2, b##_start_block, b##_end_block);		\
	if (a##_end_block < a##_start_block)				\
		printf("Invalid start/end block # for %s\n", m1);	\
	if (b##_end_block < b##_start_block)				\
		printf("Invalid start/end block # for %s\n", m2);	\
	if (chk_overlap(a, b)) {					\
		printf("%s blocks %lu..%lu overlap %s in blocks %lu..%lu!\n", \
			m1, a##_start_block, a##_end_block,		\
			m2, b##_start_block, b##_end_block);		\
		return -EINVAL;						\
	}								\
} while (0)

static int tx53_prog_uboot(void *addr, int start_block, int skip,
			size_t size, size_t max_len)
{
	int ret;
	nand_erase_options_t erase_opts = { 0, };
	size_t actual;
	size_t prg_length = max_len - skip * mtd->erasesize;
	int prg_start = (start_block + skip) * mtd->erasesize;

	erase_opts.offset = start_block * mtd->erasesize;
	erase_opts.length = max_len;
	erase_opts.quiet = 1;

	printf("Erasing flash @ %08llx..%08llx\n", erase_opts.offset,
		erase_opts.offset + erase_opts.length - 1);
	if (doit) {
		ret = nand_erase_opts(mtd, &erase_opts);
		if (ret) {
			printf("Failed to erase flash: %d\n", ret);
			return ret;
		}
	}

	printf("Programming flash @ %08x..%08x from %p\n",
		prg_start, prg_start + size - 1, addr);
	if (doit) {
		actual = size;
		ret = nand_write_skip_bad(mtd, prg_start, &actual, NULL,
					prg_length, addr, WITH_DROP_FFS);
		if (ret) {
			printf("Failed to program flash: %d\n", ret);
			return ret;
		}
		if (actual < size) {
			printf("Could only write %u of %u bytes\n", actual, size);
			return -EIO;
		}
	}
	return 0;
}

#ifdef CONFIG_ENV_IS_IN_NAND
#ifndef CONFIG_ENV_OFFSET_REDUND
#define TOTAL_ENV_SIZE CONFIG_ENV_RANGE
#else
#define TOTAL_ENV_SIZE (CONFIG_ENV_RANGE * 2)
#endif
#endif

int do_update(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	const unsigned long fcb_start_block = 0, fcb_end_block = 0;
	int erase_size = mtd->erasesize;
	void *buf;
	size_t buf_size;
	char *load_addr;
	char *file_size;
	size_t size = 0;
	void *addr = NULL;
	struct mx53_fcb *fcb;
	unsigned long mtd_num_blocks = lldiv(mtd->size, mtd->erasesize);
#ifdef CONFIG_ENV_IS_IN_NAND
	unsigned long env_start_block = CONFIG_ENV_OFFSET / mtd->erasesize;
	unsigned long env_end_block = env_start_block +
		DIV_ROUND_UP(TOTAL_ENV_SIZE, mtd->erasesize) - 1;
#endif
	int optind;
	int fw2_set = 0;
	unsigned long fw1_start_block = 0, fw1_end_block;
	unsigned long fw2_start_block = 0, fw2_end_block;
	unsigned long fw_num_blocks;
	int fw1_skip, fw2_skip;
	unsigned long extra_blocks = 0;
	u64 max_len1, max_len2;
	struct mtd_device *dev;
	struct part_info *part_info;
	struct part_info *redund_part_info;
	const char *uboot_part = "u-boot";
	const char *redund_part = NULL;
	u8 part_num;
	u8 redund_part_num;

	ret = mtdparts_init();
	if (ret)
		return ret;

	doit = true;
	for (optind = 1; optind < argc; optind++) {
		char *endp;

		if (strcmp(argv[optind], "-f") == 0) {
			if (optind >= argc - 1) {
				printf("Option %s requires an argument\n",
					argv[optind]);
				return -EINVAL;
			}
			optind++;
			fw1_start_block = simple_strtoul(argv[optind], &endp, 0);
			if (*endp != '\0') {
				uboot_part = argv[optind];
				continue;
			}
			uboot_part = NULL;
			if (fw1_start_block >= mtd_num_blocks) {
				printf("Block number %lu is out of range: 0..%lu\n",
					fw1_start_block, mtd_num_blocks - 1);
				return -EINVAL;
			}
		} else if (strcmp(argv[optind], "-r") == 0) {
			fw2_set = 1;
			if (optind < argc - 1 && argv[optind + 1][0] != '-') {
				optind++;
				fw2_start_block = simple_strtoul(argv[optind],
								&endp, 0);
				if (*endp != '\0') {
					redund_part = argv[optind];
					continue;
				}
				if (fw2_start_block >= mtd_num_blocks) {
					printf("Block number %lu is out of range: 0..%lu\n",
						fw2_start_block,
						mtd_num_blocks - 1);
					return -EINVAL;
				}
			}
		} else if (strcmp(argv[optind], "-e") == 0) {
			if (optind >= argc - 1) {
				printf("Option %s requires an argument\n",
					argv[optind]);
				return -EINVAL;
			}
			optind++;
			extra_blocks = simple_strtoul(argv[optind], NULL, 0);
			if (extra_blocks >= mtd_num_blocks) {
				printf("Extra block count %lu is out of range: 0..%lu\n",
					extra_blocks,
					mtd_num_blocks - 1);
				return -EINVAL;
			}
		} else if (strcmp(argv[optind], "-n") == 0) {
			doit = false;
		} else if (argv[optind][0] == '-') {
			printf("Unrecognized option %s\n", argv[optind]);
			return -EINVAL;
		} else {
			break;
		}
	}

	load_addr = getenv("fileaddr");
	file_size = getenv("filesize");

	if (argc - optind < 1 && load_addr == NULL) {
		printf("Load address not specified\n");
		return -EINVAL;
	}
	if (argc - optind < 2 && file_size == NULL) {
		if (uboot_part) {
			printf("WARNING: Image size not specified; overwriting whole '%s' partition\n",
				uboot_part);
			printf("This will only work, if there are no bad blocks inside this partition!\n");
		} else {
			printf("ERROR: Image size must be specified\n");
			return -EINVAL;
		}
	}
	if (argc > optind) {
		load_addr = NULL;
		addr = (void *)simple_strtoul(argv[optind], NULL, 16);
		optind++;
	}
	if (argc > optind) {
		file_size = NULL;
		size = simple_strtoul(argv[optind], NULL, 16);
		optind++;
	}
	if (load_addr != NULL) {
		addr = (void *)simple_strtoul(load_addr, NULL, 16);
		printf("Using default load address %p\n", addr);
	}
	if (file_size != NULL) {
		size = simple_strtoul(file_size, NULL, 16);
		printf("Using default file size %08x\n", size);
	}
	if (size > 0)
		fw_num_blocks = DIV_ROUND_UP(size, mtd->erasesize);
	else
		fw_num_blocks = 0;

	if (uboot_part) {
		ret = find_dev_and_part(uboot_part, &dev, &part_num,
					&part_info);
		if (ret) {
			printf("Failed to find '%s' partition: %d\n",
				uboot_part, ret);
			return ret;
		}
		fw1_start_block = lldiv(part_info->offset, mtd->erasesize);
		max_len1 = part_info->size;
		if (size == 0)
			fw_num_blocks = lldiv(max_len1, mtd->erasesize);
	} else {
		max_len1 = (u64)(fw_num_blocks + extra_blocks) * mtd->erasesize;
	}

	if (redund_part) {
		ret = find_dev_and_part(redund_part, &dev, &redund_part_num,
					&redund_part_info);
		if (ret) {
			printf("Failed to find '%s' partition: %d\n",
				redund_part, ret);
			return ret;
		}
		fw2_start_block = lldiv(redund_part_info->offset, mtd->erasesize);
		max_len2 = redund_part_info->size;
		if (fw2_start_block == fcb_start_block) {
			fw2_start_block++;
			max_len2 -= mtd->erasesize;
		}
		if (size == 0)
			fw_num_blocks = lldiv(max_len2, mtd->erasesize);
	} else if (fw2_set) {
		max_len2 = (u64)(fw_num_blocks + extra_blocks) * mtd->erasesize;
	} else {
		max_len2 = 0;
	}

	fw1_skip = find_contig_space(fw1_start_block, fw_num_blocks,
				lldiv(max_len1, mtd->erasesize));
	if (fw1_skip < 0) {
		printf("Could not find %lu contiguous good blocks for fw image in blocks %lu..%llu\n",
			fw_num_blocks, fw1_start_block,
			fw1_start_block + lldiv(max_len1, mtd->erasesize) - 1);
		if (uboot_part) {
#ifdef CONFIG_ENV_IS_IN_NAND
			if (part_info->offset <= CONFIG_ENV_OFFSET + TOTAL_ENV_SIZE) {
				printf("Use a different partition\n");
			} else {
				printf("Increase the size of the '%s' partition\n",
					uboot_part);
			}
#else
			printf("Increase the size of the '%s' partition\n",
				uboot_part);
#endif
		} else {
			printf("Increase the number of spare blocks to use with the '-e' option\n");
		}
		return -ENOSPC;
	}
	if (extra_blocks)
		fw1_end_block = fw1_start_block + fw_num_blocks + extra_blocks - 1;
	else
		fw1_end_block = fw1_start_block + fw_num_blocks + fw1_skip - 1;

	if (fw2_set && fw2_start_block == 0)
		fw2_start_block = fw1_end_block + 1;
	if (fw2_start_block > 0) {
		fw2_skip = find_contig_space(fw2_start_block, fw_num_blocks,
					lldiv(max_len2, mtd->erasesize));
		if (fw2_skip < 0) {
			printf("Could not find %lu contiguous good blocks for redundant fw image in blocks %lu..%llu\n",
				fw_num_blocks, fw2_start_block,
				fw2_start_block + lldiv(max_len2, mtd->erasesize) - 1);
			if (redund_part) {
				printf("Increase the size of the '%s' partition or use a different partition\n",
					redund_part);
			} else {
				printf("Increase the number of spare blocks to use with the '-e' option\n");
			}
			return -ENOSPC;
		}
	} else {
		fw2_skip = 0;
	}
	if (extra_blocks)
		fw2_end_block = fw2_start_block + fw_num_blocks + extra_blocks - 1;
	else
		fw2_end_block = fw2_start_block + fw_num_blocks + fw2_skip - 1;

#ifdef CONFIG_ENV_IS_IN_NAND
	fail_if_overlap(fcb, env, "FCB", "Environment");
	fail_if_overlap(fw1, env, "FW1", "Environment");
#endif
	if (fw2_set) {
		fail_if_overlap(fcb, fw2, "FCB", "FW2");
#ifdef CONFIG_ENV_IS_IN_NAND
		fail_if_overlap(fw2, env, "FW2", "Environment");
#endif
		fail_if_overlap(fw1, fw2, "FW1", "FW2");
	}
	fw1_start_block += fw1_skip;
	fw2_start_block += fw2_skip;

	buf_size = fw_num_blocks * erase_size;
	buf = memalign(erase_size, buf_size);
	if (buf == NULL) {
		printf("Failed to allocate buffer\n");
		return -ENOMEM;
	}

	/* copy U-Boot image to buffer */
	memcpy(buf, addr, size);
	memset(buf + size, 0xff, buf_size - size);

	fcb = create_fcb(buf, fw1_start_block,
			fw2_start_block, fw_num_blocks);
	if (IS_ERR(fcb)) {
		printf("Failed to initialize FCB: %ld\n", PTR_ERR(fcb));
		ret = PTR_ERR(fcb);
		goto out;
	}

	if (fw1_start_block == fcb_start_block) {
		printf("Programming FCB + U-Boot image from %p to block %lu @ %08llx\n",
			buf, fw1_start_block, (u64)fw1_start_block * mtd->erasesize);
		ret = tx53_prog_uboot(buf, fw1_start_block, fw1_skip, size,
				max_len1);
	} else {
		printf("Programming U-Boot image from %p to block %lu @ %08llx\n",
			buf, fw1_start_block, (u64)fw1_start_block * mtd->erasesize);
		ret = tx53_prog_uboot(buf, fw1_start_block, fw1_skip, size,
				max_len1);
		if (ret)
			goto out;

		memset(buf + sizeof(*fcb), 0xff,
			mtd->writesize - sizeof(*fcb));

		ret = write_fcb(buf, fcb_start_block);
		if (ret) {
			printf("Failed to write FCB to block %lu\n", fcb_start_block);
			return ret;
		}
		memset(buf, 0xff, SZ_1K);
	}
	if (ret || fw2_start_block == 0)
		goto out;

	printf("Programming redundant U-Boot image to block %lu @ %08llx\n",
		fw2_start_block, (u64)fw2_start_block * mtd->erasesize);
	ret = tx53_prog_uboot(buf, fw2_start_block, fw2_skip, size,
			max_len2);
out:
	free(buf);
	return ret;
}

U_BOOT_CMD(romupdate, 11, 0, do_update,
	"Creates an FCB data structure and writes an U-Boot image to flash",
	"[-f {<part>|block#}] [-r [{<part>|block#}]] [-e #] [<address>] [<length>]\n"
	"\t-f <part>\twrite bootloader image to partition <part>\n"
	"\t-f #\t\twrite bootloader image at block # (decimal)\n"
	"\t-r\t\twrite redundant bootloader image at next free block after first image\n"
	"\t-r <part>\twrite redundant bootloader image to partition <part>\n"
	"\t-r #\t\twrite redundant bootloader image at block # (decimal)\n"
	"\t-e #\t\tspecify number of redundant blocks per boot loader image\n"
	"\t\t\t(only valid if -f or -r specify a flash address rather than a partition name)\n"
	"\t-n\t\tshow what would be done without actually updating the flash\n"
	"\t<address>\tRAM address of bootloader image (default: ${fileaddr})\n"
	"\t<length>\tlength of bootloader image in RAM (default: ${filesize})"
	);
