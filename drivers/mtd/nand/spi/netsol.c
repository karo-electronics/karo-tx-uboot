// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * Derived from drivers/mtd/nand/spi/macronix.c
 *   Copyright (c) 2018 Macronix
 */

#ifndef __UBOOT__
#include <malloc.h>
#include <linux/device.h>
#include <linux/kernel.h>
#endif
#include <linux/bug.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_NETSOL		0x9b

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int stf1ge4u00m_ooblayout_ecc(struct mtd_info *mtd, int section,
				     struct mtd_oob_region *region)
{
	return -ERANGE;
}

static int stf1ge4u00m_ooblayout_free(struct mtd_info *mtd, int section,
				      struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	region->offset = 2;
	region->length = mtd->oobsize - 2;

	return 0;
}

static const struct mtd_ooblayout_ops stf1ge4u00m_ooblayout = {
	.ecc = stf1ge4u00m_ooblayout_ecc,
	.rfree = stf1ge4u00m_ooblayout_free,
};

static int stf1ge4u00m_get_eccsr(struct spinand_device *spinand, u8 *eccsr)
{
	struct spi_mem_op op = SPI_MEM_OP(SPI_MEM_OP_CMD(0x7c, 1),
					  SPI_MEM_OP_NO_ADDR,
					  SPI_MEM_OP_DUMMY(1, 1),
					  SPI_MEM_OP_DATA_IN(1, eccsr, 1));

	return spi_mem_exec_op(spinand->slave, &op);
}

static int stf1ge4u00m_ecc_get_status(struct spinand_device *spinand,
				      u8 status)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	u8 eccsr;

	switch (status & STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case STATUS_ECC_HAS_BITFLIPS:
		/*
		 * Let's try to retrieve the real maximum number of bitflips
		 * in order to avoid forcing the wear-leveling layer to move
		 * data around if it's not necessary.
		 */
		if (stf1ge4u00m_get_eccsr(spinand, &eccsr))
			return nand->eccreq.strength;

		if (WARN_ON(eccsr > nand->eccreq.strength || !eccsr))
			return nand->eccreq.strength;

		return eccsr;

	default:
		break;
	}

	return -EINVAL;
}

static const struct spinand_info netsol_spinand_table[] = {
	SPINAND_INFO("STF1GE4U00M", 0x12,
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 1, 1, 1),
		     NAND_ECCREQ(1, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&stf1ge4u00m_ooblayout,
				     stf1ge4u00m_ecc_get_status)),
};

static int netsol_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	/*
	 * Netsol SPI NAND read ID needs a dummy byte, so the first byte in
	 * raw_id is garbage.
	 */
	if (id[1] != SPINAND_MFR_NETSOL)
		return 0;

	ret = spinand_match_and_init(spinand, netsol_spinand_table,
				     ARRAY_SIZE(netsol_spinand_table),
				     id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops netsol_spinand_manuf_ops = {
	.detect = netsol_spinand_detect,
};

const struct spinand_manufacturer netsol_spinand_manufacturer = {
	.id = SPINAND_MFR_NETSOL,
	.name = "Netsol",
	.ops = &netsol_spinand_manuf_ops,
};
