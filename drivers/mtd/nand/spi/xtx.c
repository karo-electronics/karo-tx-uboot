// SPDX-License-Identifier: GPL-2.0
/*
 * Author:
 * Felix Matouschek <felix@matouschek.org>
 */

#ifndef __UBOOT__
#include <linux/device.h>
#include <linux/kernel.h>
#endif
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_XTX	0x0B

#define XT26G0XA_STATUS_ECC_MASK	GENMASK(5, 2)
#define XT26G0XA_STATUS_ECC_NO_DETECTED	(0 << 2)
#define XT26G0XA_STATUS_ECC_8_CORRECTED	(3 << 4)
#define XT26G0XA_STATUS_ECC_UNCOR_ERROR	(2 << 4)

#define XT26G0XC_STATUS_ECC_SHIFT	4
#define XT26G0XC_STATUS_ECC_NO_DETECTED	(0 << XT26G0XC_STATUS_ECC_SHIFT)
#define XT26G0XC_STATUS_ECC_MASK	GENMASK(7, XT26G0XC_STATUS_ECC_SHIFT)
#define XT26G0XC_STATUS_ECC_UNCOR_ERROR	(0xf << XT26G0XC_STATUS_ECC_SHIFT)

static SPINAND_OP_VARIANTS(read_cache_variants,
			   SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 1, NULL, 0),
			   SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
			   SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),
			   SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
			   SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
			   SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
			   SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
			   SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
			   SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
			   SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int xt26g0xa_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 48;
	region->length = 16;

	return 0;
}

static int xt26g0xa_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 8;
	region->length = 40;

	return 0;
}

static const struct mtd_ooblayout_ops xt26g0xa_ooblayout = {
	.ecc = xt26g0xa_ooblayout_ecc,
	.rfree = xt26g0xa_ooblayout_free,
};

static int xt26g0xc_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 64;
	region->length = 52;

	return 0;
}

static int xt26g0xc_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 1;
	region->length = 63;

	return 0;
}

static const struct mtd_ooblayout_ops xt26g0xc_ooblayout = {
	.ecc = xt26g0xc_ooblayout_ecc,
	.rfree = xt26g0xc_ooblayout_free,
};

static int xt26g0xa_ecc_get_status(struct spinand_device *spinand,
				   u8 status)
{
	status = status & XT26G0XA_STATUS_ECC_MASK;

	switch (status) {
	case XT26G0XA_STATUS_ECC_NO_DETECTED:
		return 0;
	case XT26G0XA_STATUS_ECC_8_CORRECTED:
		return 8;
	case XT26G0XA_STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;
	default:
		break;
	}

	/* At this point values greater than (2 << 4) are invalid  */
	if (status > XT26G0XA_STATUS_ECC_UNCOR_ERROR)
		return -EINVAL;

	/* (1 << 2) through (7 << 2) are 1-7 corrected errors */
	return status >> 2;
}

static int xt26g0xc_ecc_get_status(struct spinand_device *spinand,
				   u8 status)
{
	status = status & XT26G0XC_STATUS_ECC_MASK;

	switch (status) {
	case XT26G0XC_STATUS_ECC_NO_DETECTED:
		return 0;
	case XT26G0XC_STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;
	default:
		break;
	}

	status >>= XT26G0XC_STATUS_ECC_SHIFT;

	/* 1 through 8 are 1-8 corrected errors; everything else is invalid */
	return status > 8 ? -EINVAL : status;
}

static const struct spinand_info xtx_spinand_table[] = {
	SPINAND_INFO("XT26G01A", 0xe1,
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&xt26g0xa_ooblayout,
				     xt26g0xa_ecc_get_status)),
	SPINAND_INFO("XT26G01C", 0x11,
		     NAND_MEMORG(1, 2048, 128, 64, 1024, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&xt26g0xc_ooblayout,
				     xt26g0xc_ecc_get_status)),
	SPINAND_INFO("XT26G02A", 0xe2,
		     NAND_MEMORG(1, 2048, 64, 64, 2048, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&xt26g0xa_ooblayout,
				     xt26g0xa_ecc_get_status)),
	SPINAND_INFO("XT26G04A", 0xe3,
		     NAND_MEMORG(1, 2048, 64, 128, 2048, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&xt26g0xa_ooblayout,
				     xt26g0xa_ecc_get_status)),
};

static int xtx_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	/*
	 * XTX SPI NAND read ID needs a dummy byte, so the first byte in
	 * raw_id is garbage.
	 */
	if (id[1] != SPINAND_MFR_XTX)
		return 0;

	ret = spinand_match_and_init(spinand, xtx_spinand_table,
				     ARRAY_SIZE(xtx_spinand_table),
				     id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops xtx_spinand_manuf_ops = {
	.detect = xtx_spinand_detect,
};

const struct spinand_manufacturer xtx_spinand_manufacturer = {
	.id = SPINAND_MFR_XTX,
	.name = "XTX",
	.ops = &xtx_spinand_manuf_ops,
};
