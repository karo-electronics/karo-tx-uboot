/*
 * Copyright (C) 2011-2014 Lothar Waßmann <LW@KARO-electronics.de>
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

#include <asm/io.h>
#include <asm/sizes.h>
#include <asm/arch/regs-base.h>
#include <asm/imx-common/regs-gpmi.h>
#include <asm/imx-common/regs-bch.h>

struct mx28_nand_timing {
	u8 data_setup;
	u8 data_hold;
	u8 address_setup;
	u8 dsample_time;
	u8 nand_timing_state;
	u8 tREA;
	u8 tRLOH;
	u8 tRHOH;
};

struct mx28_fcb {
	u32 checksum;
	u32 fingerprint;
	u32 version;
	struct mx28_nand_timing timing;
	u32 page_data_size;
	u32 total_page_size;
	u32 sectors_per_block;
	u32 number_of_nands;	/* not used by ROM code */
	u32 total_internal_die;	/* not used by ROM code */
	u32 cell_type;		/* not used by ROM code */
	u32 ecc_blockn_type;
	u32 ecc_block0_size;
	u32 ecc_blockn_size;
	u32 ecc_block0_type;
	u32 metadata_size;
	u32 ecc_blocks_per_page;
	u32 rsrvd[6];		 /* not used by ROM code */
	u32 bch_mode;
	u32 boot_patch;
	u32 patch_sectors;
	u32 fw1_start_page;
	u32 fw2_start_page;
	u32 fw1_sectors;
	u32 fw2_sectors;
	u32 dbbt_search_area;
	u32 bb_mark_byte;
	u32 bb_mark_startbit;
	u32 bb_mark_phys_offset;
};

#define BF_VAL(v, bf)		(((v) & bf##_MASK) >> bf##_OFFSET)

static nand_info_t *mtd = &nand_info[0];

extern void *_start;

#define BIT(v,n)	(((v) >> (n)) & 0x1)

static u8 calculate_parity_13_8(u8 d)
{
	u8 p = 0;

	p |= (BIT(d, 6) ^ BIT(d, 5) ^ BIT(d, 3) ^ BIT(d, 2))		 << 0;
	p |= (BIT(d, 7) ^ BIT(d, 5) ^ BIT(d, 4) ^ BIT(d, 2) ^ BIT(d, 1)) << 1;
	p |= (BIT(d, 7) ^ BIT(d, 6) ^ BIT(d, 5) ^ BIT(d, 1) ^ BIT(d, 0)) << 2;
	p |= (BIT(d, 7) ^ BIT(d, 4) ^ BIT(d, 3) ^ BIT(d, 0))		 << 3;
	p |= (BIT(d, 6) ^ BIT(d, 4) ^ BIT(d, 3) ^ BIT(d, 2) ^ BIT(d, 1) ^ BIT(d, 0)) << 4;
	return p;
}

static void encode_hamming_13_8(void *_src, void *_ecc, size_t size)
{
	int i;
	u8 *src = _src;
	u8 *ecc = _ecc;

	for (i = 0; i < size; i++)
		ecc[i] = calculate_parity_13_8(src[i]);
}

static u32 calc_chksum(void *buf, size_t size)
{
	u32 chksum = 0;
	u8 *bp = buf;
	size_t i;

	for (i = 0; i < size; i++) {
		chksum += bp[i];
	}
	return ~chksum;
}

/*
  Physical organisation of data in NAND flash:
  metadata
  payload chunk 0 (may be empty)
  ecc for metadata + payload chunk 0
  payload chunk 1
  ecc for payload chunk 1
...
  payload chunk n
  ecc for payload chunk n
 */

static int calc_bb_offset(nand_info_t *mtd, struct mx28_fcb *fcb)
{
	int bb_mark_offset;
	int chunk_data_size = fcb->ecc_blockn_size * 8;
	int chunk_ecc_size = (fcb->ecc_blockn_type << 1) * 13;
	int chunk_total_size = chunk_data_size + chunk_ecc_size;
	int bb_mark_chunk, bb_mark_chunk_offs;

	bb_mark_offset = (mtd->writesize - fcb->metadata_size) * 8;
	if (fcb->ecc_block0_size == 0)
		bb_mark_offset -= (fcb->ecc_block0_type << 1) * 13;

	bb_mark_chunk = bb_mark_offset / chunk_total_size;
	bb_mark_chunk_offs = bb_mark_offset - (bb_mark_chunk * chunk_total_size);
	if (bb_mark_chunk_offs > chunk_data_size) {
		printf("Unsupported ECC layout; BB mark resides in ECC data: %u\n",
			bb_mark_chunk_offs);
		return -EINVAL;
	}
	bb_mark_offset -= bb_mark_chunk * chunk_ecc_size;
	return bb_mark_offset;
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

#define pr_fcb_val(p, n)	debug("%s=%08x(%d)\n", #n, (p)->n, (p)->n)

static struct mx28_fcb *create_fcb(void *buf, int fw1_start_block,
				int fw2_start_block, int fw_num_blocks)
{
	struct gpmi_regs *gpmi_base = (void *)GPMI_BASE_ADDRESS;
	struct bch_regs *bch_base = (void *)BCH_BASE_ADDRESS;
	u32 fl0, fl1;
	u32 t0;
	int metadata_size;
	int bb_mark_bit_offs;
	struct mx28_fcb *fcb;
	int fcb_offs;

	if (gpmi_base == NULL || bch_base == NULL) {
		return ERR_PTR(-ENOMEM);
	}

	fl0 = readl(&bch_base->hw_bch_flash0layout0);
	fl1 = readl(&bch_base->hw_bch_flash0layout1);
	t0 = readl(&gpmi_base->hw_gpmi_timing0);

	metadata_size = BF_VAL(fl0, BCH_FLASHLAYOUT0_META_SIZE);

	fcb = buf + ALIGN(metadata_size, 4);
	fcb_offs = (void *)fcb - buf;

	memset(buf, 0xff, fcb_offs);
	memset(fcb, 0x00, sizeof(*fcb));
	memset(fcb + 1, 0xff, mtd->erasesize - fcb_offs - sizeof(*fcb));

	strncpy((char *)&fcb->fingerprint, "FCB ", 4);
	fcb->version = cpu_to_be32(1);

	/* ROM code assumes GPMI clock of 25 MHz */
	fcb->timing.data_setup = BF_VAL(t0, GPMI_TIMING0_DATA_SETUP) * 40;
	fcb->timing.data_hold = BF_VAL(t0, GPMI_TIMING0_DATA_HOLD) * 40;
	fcb->timing.address_setup = BF_VAL(t0, GPMI_TIMING0_ADDRESS_SETUP) * 40;

	fcb->page_data_size = mtd->writesize;
	fcb->total_page_size = mtd->writesize + mtd->oobsize;
	fcb->sectors_per_block = mtd->erasesize / mtd->writesize;

	fcb->ecc_block0_type = BF_VAL(fl0, BCH_FLASHLAYOUT0_ECC0);
	fcb->ecc_block0_size = BF_VAL(fl0, BCH_FLASHLAYOUT0_DATA0_SIZE);
	fcb->ecc_blockn_type = BF_VAL(fl1, BCH_FLASHLAYOUT1_ECCN);
	fcb->ecc_blockn_size = BF_VAL(fl1, BCH_FLASHLAYOUT1_DATAN_SIZE);

	pr_fcb_val(fcb, ecc_block0_type);
	pr_fcb_val(fcb, ecc_blockn_type);
	pr_fcb_val(fcb, ecc_block0_size);
	pr_fcb_val(fcb, ecc_blockn_size);

	fcb->metadata_size = BF_VAL(fl0, BCH_FLASHLAYOUT0_META_SIZE);
	fcb->ecc_blocks_per_page = BF_VAL(fl0, BCH_FLASHLAYOUT0_NBLOCKS);
	fcb->bch_mode = readl(&bch_base->hw_bch_mode);
/*
	fcb->boot_patch = 0;
	fcb->patch_sectors = 0;
*/
	fcb->fw1_start_page = fw1_start_block * fcb->sectors_per_block;
	fcb->fw1_sectors = fw_num_blocks * fcb->sectors_per_block;
	pr_fcb_val(fcb, fw1_start_page);
	pr_fcb_val(fcb, fw1_sectors);

	if (fw2_start_block != 0 && fw2_start_block < mtd->size / mtd->erasesize) {
		fcb->fw2_start_page = fw2_start_block * fcb->sectors_per_block;
		fcb->fw2_sectors = fcb->fw1_sectors;
		pr_fcb_val(fcb, fw2_start_page);
		pr_fcb_val(fcb, fw2_sectors);
	}

	fcb->dbbt_search_area = 1;

	bb_mark_bit_offs = calc_bb_offset(mtd, fcb);
	if (bb_mark_bit_offs < 0)
		return ERR_PTR(bb_mark_bit_offs);
	fcb->bb_mark_byte = bb_mark_bit_offs / 8;
	fcb->bb_mark_startbit = bb_mark_bit_offs % 8;
	fcb->bb_mark_phys_offset = mtd->writesize;

	pr_fcb_val(fcb, bb_mark_byte);
	pr_fcb_val(fcb, bb_mark_startbit);
	pr_fcb_val(fcb, bb_mark_phys_offset);

	fcb->checksum = calc_chksum(&fcb->fingerprint, 512 - 4);
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

	ret = nand_erase(mtd, block * mtd->erasesize, mtd->erasesize);
	if (ret) {
		printf("Failed to erase FCB block %u\n", block);
		return ret;
	}

	printf("Writing FCB to block %d @ %08llx\n", block,
		(u64)block * mtd->erasesize);
	chip->select_chip(mtd, 0);
	ret = chip->write_page(mtd, chip, buf, 1, page, 0, 1);
	if (ret) {
		printf("Failed to write FCB to block %u: %d\n", block, ret);
	}
	chip->select_chip(mtd, -1);
	return ret;
}

#define chk_overlap(a,b)				\
	((a##_start_block <= b##_end_block &&		\
		a##_end_block >= b##_start_block) ||	\
	(b##_start_block <= a##_end_block &&		\
		b##_end_block >= a##_start_block))

#define fail_if_overlap(a,b,m1,m2) do {				\
	if (chk_overlap(a, b)) {				\
		printf("%s blocks %lu..%lu overlap %s in blocks %lu..%lu!\n", \
			m1, a##_start_block, a##_end_block,	\
			m2, b##_start_block, b##_end_block);	\
		return -EINVAL;					\
	}							\
} while (0)

static int tx28_prog_uboot(void *addr, int start_block, int skip,
			size_t size, size_t max_len)
{
	int ret;
	nand_erase_options_t erase_opts = { 0, };
	size_t actual;

	erase_opts.offset = start_block * mtd->erasesize;
	erase_opts.length = max_len;
	erase_opts.quiet = 1;

	printf("Erasing flash @ %08llx..%08llx\n", erase_opts.offset,
		erase_opts.offset + erase_opts.length - 1);
	ret = nand_erase_opts(mtd, &erase_opts);
	if (ret) {
		printf("Failed to erase flash: %d\n", ret);
		return ret;
	}

	printf("Programming flash @ %08llx..%08llx from %p\n",
		(u64)start_block * mtd->erasesize,
		(u64)start_block * mtd->erasesize + size - 1, addr);
	actual = size;
	ret = nand_write_skip_bad(mtd, start_block * mtd->erasesize,
				&actual, NULL, erase_opts.length, addr,
				WITH_DROP_FFS);
	if (ret) {
		printf("Failed to program flash: %d\n", ret);
		return ret;
	}
	if (actual < size) {
		printf("Could only write %u of %u bytes\n", actual, size);
		return -EIO;
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
	int page_size = mtd->writesize;
	void *buf;
	char *load_addr;
	char *file_size;
	size_t size = 0;
	void *addr = NULL;
	struct mx28_fcb *fcb;
	unsigned long mtd_num_blocks = mtd->size / mtd->erasesize;
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
	size_t max_len1, max_len2;
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
		printf("WARNING: Image size not specified; overwriting whole uboot partition\n");
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
	if (size > 0) {
		fw_num_blocks = DIV_ROUND_UP(size, mtd->erasesize);
	} else {
		fw_num_blocks = part_info->size / mtd->erasesize -
			extra_blocks;
		size = fw_num_blocks * mtd->erasesize;
	}

	if (uboot_part) {
		ret = find_dev_and_part(uboot_part, &dev, &part_num,
					&part_info);
		if (ret) {
			printf("Failed to find '%s' partition: %d\n",
				uboot_part, ret);
			return ret;
		}
		fw1_start_block = part_info->offset / mtd->erasesize;
		max_len1 = part_info->size;
	} else {
		max_len1 = (fw_num_blocks + extra_blocks) * mtd->erasesize;
	}

	if (redund_part) {
		ret = find_dev_and_part(redund_part, &dev, &redund_part_num,
					&redund_part_info);
		if (ret) {
			printf("Failed to find '%s' partition: %d\n",
				redund_part, ret);
			return ret;
		}
		fw2_start_block = redund_part_info->offset / mtd->erasesize;
		max_len2 = redund_part_info->size;
	} else if (fw2_set) {
		max_len2 = (fw_num_blocks + extra_blocks) * mtd->erasesize;
	} else {
		max_len2 = 0;
	}

	fw1_skip = find_contig_space(fw1_start_block, fw_num_blocks,
				max_len1 / mtd->erasesize);
	if (fw1_skip < 0) {
		printf("Could not find %lu contiguous good blocks for fw image\n",
			fw_num_blocks);
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
	fw1_end_block = fw1_start_block + fw1_skip + fw_num_blocks - 1;

	if (fw2_set && fw2_start_block == 0)
		fw2_start_block = fw1_end_block + 1;
	if (fw2_start_block > 0) {
		fw2_skip = find_contig_space(fw2_start_block, fw_num_blocks,
					max_len2 / mtd->erasesize);
		if (fw2_skip < 0) {
			printf("Could not find %lu contiguous good blocks for redundant fw image\n",
				fw_num_blocks);
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
	fw2_end_block = fw2_start_block + fw2_skip + fw_num_blocks - 1;

#ifdef CONFIG_ENV_IS_IN_NAND
	fail_if_overlap(fcb, env, "FCB", "Environment");
	fail_if_overlap(fw1, env, "FW1", "Environment");
#endif
	fail_if_overlap(fcb, fw1, "FCB", "FW1");
	if (fw2_set) {
		fail_if_overlap(fcb, fw2, "FCB", "FW2");
#ifdef CONFIG_ENV_IS_IN_NAND
		fail_if_overlap(fw2, env, "FW2", "Environment");
#endif
		fail_if_overlap(fw1, fw2, "FW1", "FW2");
	}

	buf = malloc(erase_size);
	if (buf == NULL) {
		printf("Failed to allocate buffer\n");
		return -ENOMEM;
	}

	fcb = create_fcb(buf, fw1_start_block + fw1_skip,
			fw2_start_block + fw2_skip, fw_num_blocks);
	if (IS_ERR(fcb)) {
		printf("Failed to initialize FCB: %ld\n", PTR_ERR(fcb));
		free(buf);
		return PTR_ERR(fcb);
	}
	encode_hamming_13_8(fcb, (void *)fcb + 512, 512);

	ret = write_fcb(buf, fcb_start_block);
	free(buf);
	if (ret) {
		printf("Failed to write FCB to block %lu\n", fcb_start_block);
		return ret;
	}

	if (size & (page_size - 1)) {
		memset(addr + size, 0xff, size & (page_size - 1));
		size = ALIGN(size, page_size);
	}

	printf("Programming U-Boot image from %p to block %lu @ %08llx\n",
		addr, fw1_start_block + fw1_skip,
		(u64)(fw1_start_block + fw1_skip) * mtd->erasesize);
	ret = tx28_prog_uboot(addr, fw1_start_block, fw1_skip, size,
			max_len1);

	if (fw2_start_block == 0) {
		return ret;
	}

	printf("Programming redundant U-Boot image to block %lu @ %08llx\n",
		fw2_start_block + fw2_skip,
		(u64)(fw2_start_block + fw2_skip) * mtd->erasesize);
	ret = tx28_prog_uboot(addr, fw2_start_block, fw2_skip, fw_num_blocks,
			max_len2);
	return ret;
}

U_BOOT_CMD(romupdate, 11, 0, do_update,
	"Creates an FCB data structure and writes an U-Boot image to flash",
	"[-f {<part>|block#}] [-r [{<part>|block#}]] [-e #] [<address>] [<length>]\n"
	"\t-f <part>\twrite bootloader image to partition <part>\n"
	"\t-f #\twrite bootloader image at block # (decimal)\n"
	"\t-r\twrite redundant bootloader image at next free block after first image\n"
	"\t-r <part>\twrite redundant bootloader image to partition <part>\n"
	"\t-r #\twrite redundant bootloader image at block # (decimal)\n"
	"\t-e #\tspecify number of redundant blocks per boot loader image\n"
	"\t\tonly valid if -f or -r specify a flash address rather than a partition name\n"
	"\t<address>\tRAM address of bootloader image (default: ${fileaddr}\n"
	"\t<length>\tlength of bootloader image in RAM (default: ${filesize}"
	);
