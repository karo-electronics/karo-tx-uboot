// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020  Lothar Waßmann <LW@KARO-electronics.de>
 * based on: spl_nand.c (C) 2011 Corscience GmbH & Co. KG - Simon Schwarz <schwarz@corscience.de>
 */

#include <common.h>
#include <config.h>
#include <fdt.h>
#include <fdt_support.h>
#include <image.h>
#include <log.h>
#include <mtd.h>
#include <spl.h>
#include <dm/uclass.h>
#include <linux/libfdt_env.h>
#include <linux/mtd/mtd.h>

static struct mtd_info *spl_mtd_init(void)
{
	int ret;
	struct mtd_info *mtd;

	ret = mtd_probe_devices();
	if (ret) {
		printf("Failed to probe MTD devices: %d\n", ret);
		return ERR_PTR(ret);
	}

	mtd = get_mtd_device_nm(CONFIG_SPL_MTD_BOOT_DEVICE);
	if (IS_ERR(mtd)) {
		printf("failed to find 'ssbl' partition: %ld\n",
		       PTR_ERR(mtd));
	}

	return mtd;
}

int mtd_spl_load_image(struct mtd_info *mtd, uint32_t offs, unsigned int size,
		       void *dst)
{
	int ret;
	size_t read;

	ret = mtd_read(mtd, offs, size, &read, dst);
	debug("%s: mtd_read(0x%08x, 0x%08x, 0x%p) returned %d (%zu)\n",
	      __func__, offs, size, dst, ret, read);
	return ret;
}

static ulong spl_mtd_fit_read(struct spl_load_info *load,
			      ulong offs, ulong size, void *dst)
{
	int ret;
	struct mtd_info *mtd = spl_mtd_init();

	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	ret = mtd_spl_load_image(mtd, offs, size, dst);
	if (!ret)
		return size;
	else
		return 0;
}

static int spl_mtd_load_element(struct spl_image_info *spl_image,
				int offset, struct image_header *header)
{
	int ret;
	struct mtd_info *mtd = spl_mtd_init();

	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	ret = mtd_spl_load_image(mtd, offset, sizeof(*header), header);
	if (ret)
		return ret;

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = NULL;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = 1;
		load.read = spl_mtd_fit_read;
		return spl_load_simple_fit(spl_image, &load, offset, header);
	}
	if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER)) {
		struct spl_load_info load;

		load.dev = NULL;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = 1;
		load.read = spl_mtd_fit_read;
		return spl_load_imx_container(spl_image, &load, offset);
	}
	ret = spl_parse_image_header(spl_image, header);
	if (ret)
		return ret;
	return mtd_spl_load_image(mtd, offset, spl_image->size,
				  (void *)spl_image->load_addr);
}

static int spl_mtd_load_image(struct spl_image_info *spl_image,
			      struct spl_boot_device *bootdev)
{
	int ret;
	struct image_header *header;
	struct mtd_info *mtd = spl_mtd_init();

	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	header = spl_get_load_buffer(0, sizeof(*header));

	/* Load u-boot */
	ret = spl_mtd_load_element(spl_image, 0, header);
	put_mtd_device(mtd);
	return ret;
}

/* Use priority 1 so that UBI can override this */
SPL_LOAD_IMAGE_METHOD("MTD", 1, BOOT_DEVICE_MTD, spl_mtd_load_image);
