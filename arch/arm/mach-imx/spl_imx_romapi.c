// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 NXP
 */

#include <common.h>
#include <errno.h>
#include <image.h>
#include <log.h>
#include <malloc.h>
#include <spl.h>
#include <asm/arch/sys_proto.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <asm/mach-imx/image.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/libfdt.h>

DECLARE_GLOBAL_DATA_PTR;

/* Caller need ensure the offset and size to align with page size */
ulong spl_romapi_raw_seekable_read(u32 offset, u32 size, void *buf)
{
	volatile gd_t *pgd = gd;
	int ret;

	debug("%s 0x%x, size 0x%x\n", __func__, offset, size);

	ret = g_rom_api->download_image(buf, offset, size,
					((uintptr_t)buf) ^ offset ^ size);

	set_gd(pgd);

	if (ret == ROM_API_OKAY)
		return size;

	printf("%s Failure when load 0x%x, size 0x%x\n", __func__, offset, size);

	return 0;
}

ulong __weak spl_romapi_get_uboot_base(u32 image_offset, u32 rom_bt_dev)
{
	u32 offset;

	if (((rom_bt_dev >> 16) & 0xff) ==  BT_DEV_TYPE_FLEXSPINOR)
		offset = CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512;
	else
		offset = image_offset + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512 - 0x8000;

	return offset;
}

static int is_boot_from_stream_device(u32 boot)
{
	u32 interface;

	interface = boot >> 16;
	if (interface >= BT_DEV_TYPE_USB)
		return 1;

	if (interface == BT_DEV_TYPE_MMC && (boot & 1))
		return 1;

	return 0;
}

static ulong spl_romapi_read_seekable(struct spl_load_info *load,
				      ulong sector, ulong count,
				      void *buf)
{
	u32 pagesize = *(u32 *)load->priv;
	ulong byte = count * pagesize;
	u32 offset;
#define RSRVD_ADDR		((void *)0x980000)
#define RSRVD_LEN		((void *)0x990000 - RSRVD_ADDR)

	offset = sector * pagesize;

	/* Handle corner case for ocram 0x980000 to 0x98ffff ecc region, ROM does not allow to access it */
	if (is_imx8mp()) {
		ulong ret;
		void *new_buf;

		if ((buf >= RSRVD_ADDR && buf < RSRVD_ADDR + RSRVD_LEN)) {
			new_buf = memalign(ARCH_DMA_MINALIGN, byte);
			if (!new_buf) {
				printf("%s: Failed to allocate bounce buffer\n", __func__);
				return 0;
			}
			ret = spl_romapi_raw_seekable_read(offset, byte, new_buf);
			memcpy(buf, new_buf, ret);
			free(new_buf);
			return ret / pagesize;
		} else if (buf + byte + pagesize >= RSRVD_ADDR &&
			   buf + byte < RSRVD_ADDR + RSRVD_LEN) {
			ulong ret2;
			u32 over_size = ALIGN(buf + byte - RSRVD_ADDR + 1, pagesize);

			ret = spl_romapi_raw_seekable_read(offset, byte - over_size, buf);
			new_buf = memalign(ARCH_DMA_MINALIGN, over_size);
			if (!new_buf) {
				printf("%s: Failed to allocate bounce buffer\n", __func__);
				return 0;
			}

			ret2 = spl_romapi_raw_seekable_read(offset + byte - over_size,
							    over_size, new_buf);
			memcpy(buf + byte - over_size, new_buf, ret2);
			free(new_buf);
			return ret && ret2 ? (ret + ret2) / pagesize : 0;
		}
	}

	return spl_romapi_raw_seekable_read(offset, byte, buf) / pagesize;
}

static int spl_romapi_load_image_seekable(struct spl_image_info *spl_image,
					  struct spl_boot_device *bootdev,
					  u32 rom_bt_dev)
{
	volatile gd_t *pgd = gd;
	int ret;
	u32 offset;
	u32 pagesize, size;
	struct image_header *header;
	u32 image_offset;

	ret = g_rom_api->query_boot_infor(QUERY_IVT_OFF, &offset,
					  ((uintptr_t)&offset) ^ QUERY_IVT_OFF);
	ret |= g_rom_api->query_boot_infor(QUERY_PAGE_SZ, &pagesize,
					   ((uintptr_t)&pagesize) ^ QUERY_PAGE_SZ);
	ret |= g_rom_api->query_boot_infor(QUERY_IMG_OFF, &image_offset,
					   ((uintptr_t)&image_offset) ^ QUERY_IMG_OFF);

	set_gd(pgd);

	if (ret != ROM_API_OKAY) {
		puts("ROMAPI: Failure query boot infor pagesize/offset\n");
		return -1;
	}

	header = (struct image_header *)(CONFIG_SPL_IMX_ROMAPI_LOADADDR);

	printf("image offset 0x%x, pagesize 0x%x, ivt offset 0x%x\n",
	       image_offset, pagesize, offset);

	offset = spl_romapi_get_uboot_base(image_offset, rom_bt_dev);

	size = ALIGN(sizeof(struct image_header), pagesize);
	ret = g_rom_api->download_image((u8 *)header, offset, size,
					((uintptr_t)header) ^ offset ^ size);
	set_gd(pgd);

	if (ret != ROM_API_OKAY) {
		printf("ROMAPI: download failure offset 0x%x size 0x%x\n",
		       offset, size);
		return -1;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) && image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		memset(&load, 0, sizeof(load));
		load.bl_len = pagesize;
		load.read = spl_romapi_read_seekable;
		load.priv = &pagesize;
		return spl_load_simple_fit(spl_image, &load, offset / pagesize, header);
	} else if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER)) {
		struct spl_load_info load;

		memset(&load, 0, sizeof(load));
		load.bl_len = pagesize;
		load.read = spl_romapi_read_seekable;
		load.priv = &pagesize;

		ret = spl_load_imx_container(spl_image, &load, offset / pagesize);
	} else {
		/* TODO */
		puts("Can't support legacy image\n");
		return -1;
	}

	return 0;
}

static ulong spl_ram_load_read(struct spl_load_info *load, ulong sector,
			       ulong count, void *buf)
{
	memcpy(buf, (void *)(sector), count);

	if (load->priv) {
		ulong *p = (ulong *)load->priv;
		ulong total = sector + count;

		if (total > *p)
			*p = total;
	}

	return count;
}

static ulong get_fit_image_size(void *fit)
{
	struct spl_image_info spl_image;
	struct spl_load_info spl_load_info;
	ulong last = (ulong)fit;

	memset(&spl_load_info, 0, sizeof(spl_load_info));
	spl_load_info.bl_len = 1;
	spl_load_info.read = spl_ram_load_read;
	spl_load_info.priv = &last;

	/* We call load_simple_fit is just to get total size, the image is not downloaded,
         * so should bypass authentication
         */
	spl_image.flags = SPL_FIT_BYPASS_POST_LOAD;
	spl_load_simple_fit(&spl_image, &spl_load_info,
			    (uintptr_t)fit, fit);

	return last - (ulong)fit;
}

static u8 *search_fit_header(u8 *p, int size)
{
	int i;

	for (i = 0; i < size; i += 4)
		if (genimg_get_format(p + i) == IMAGE_FORMAT_FIT)
			return p + i;

	return NULL;
}

static u8 *search_container_header(u8 *p, int size)
{
	int i = 0;
	u8 *hdr;

	for (i = 0; i < size; i += 4) {
		hdr = p + i;
		if (*(hdr + 3) == 0x87 && *hdr == 0 && (*(hdr + 1) != 0 || *(hdr + 2) != 0))
			return p + i;
	}

	return NULL;
}

static u8 *search_img_header(u8 *p, int size)
{
	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT))
		return search_fit_header(p, size);
	else if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER))
		return search_container_header(p, size);

	return NULL;
}

static u32 img_header_size(void)
{
	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT))
		return sizeof(struct fdt_header);
	else if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER))
		return sizeof(struct container_hdr);

	return 0;
}

static int img_info_size(void *img_hdr)
{
#ifdef CONFIG_SPL_LOAD_FIT
	return fit_get_size(img_hdr);
#elif defined CONFIG_SPL_LOAD_IMX_CONTAINER
	struct container_hdr *container = img_hdr;

	return (container->length_lsb + (container->length_msb << 8));
#else
	return 0;
#endif
}

static int img_total_size(void *img_hdr)
{
	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT)) {
		return get_fit_image_size(img_hdr);
	} else if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER)) {
		int total = get_container_size((ulong)img_hdr, NULL);

		if (total < 0) {
			printf("invalid container image\n");
			return 0;
		}

		return total;
	}

	return 0;
}

static int spl_romapi_load_image_stream(struct spl_image_info *spl_image,
					struct spl_boot_device *bootdev)
{
	struct spl_load_info load;
	volatile gd_t *pgd = gd;
	u32 pagesize, pg;
	int ret;
	int i = 0;
	u8 *p = (u8 *)CONFIG_SPL_IMX_ROMAPI_LOADADDR;
	u8 *phdr = NULL;
	int imagesize;
	int total;

	ret = g_rom_api->query_boot_infor(QUERY_PAGE_SZ, &pagesize,
					  ((uintptr_t)&pagesize) ^ QUERY_PAGE_SZ);
	set_gd(pgd);

	if (ret != ROM_API_OKAY)
		puts("failure at query_boot_info\n");

	pg = pagesize;
	if (pg < 1024)
		pg = 1024;

	for (i = 0; i < 640; i++) {
		ret = g_rom_api->download_image(p, 0, pg,
						((uintptr_t)p) ^ pg);
		set_gd(pgd);

		if (ret != ROM_API_OKAY) {
			puts("Stream(USB) download failure\n");
			return -1;
		}

		phdr = search_img_header(p, pg);
		p += pg;

		if (phdr)
			break;
	}

	if (!phdr) {
		puts("Could not find U-Boot image header within first 640KiB of image\n");
		return -1;
	}

	if (p - phdr < img_header_size()) {
		ret = g_rom_api->download_image(p, 0, pg, ((uintptr_t)p) ^ pg);
		set_gd(pgd);

		if (ret != ROM_API_OKAY) {
			puts("Stream(USB) download failure\n");
			return -1;
		}

		p += pg;
	}

	imagesize = img_info_size(phdr);
	printf("Find img info 0x%p, size %d\n", phdr, imagesize);

	if (p - phdr < imagesize) {
		imagesize -= p - phdr;
		/* need pagesize here after ROM fix USB problem */
		imagesize = ALIGN(imagesize, pg);

		printf("Need to continue download %u\n", imagesize);

		ret = g_rom_api->download_image(p, 0, imagesize,
						((uintptr_t)p) ^ imagesize);
		set_gd(pgd);

		p += imagesize;

		if (ret != ROM_API_OKAY) {
			printf("Download failed: %d\n", imagesize);
			return -1;
		}
	}

	total = ALIGN(img_total_size(phdr), 4);

	imagesize = ALIGN(total - (p - phdr), pagesize);

	printf("Download %d, Total size %d\n", imagesize, total);

	ret = g_rom_api->download_image(p, 0, imagesize,
					((uintptr_t)p) ^ imagesize);
	set_gd(pgd);
	if (ret != ROM_API_OKAY)
		printf("ROMAPI: download failure %d\n", imagesize);

	memset(&load, 0, sizeof(load));
	load.bl_len = 1;
	load.read = spl_ram_load_read;

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT))
		return spl_load_simple_fit(spl_image, &load, (ulong)phdr, phdr);
	else if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER))
		return spl_load_imx_container(spl_image, &load, (ulong)phdr);

	return -1;
}

int board_return_to_bootrom(struct spl_image_info *spl_image,
			    struct spl_boot_device *bootdev)
{
	volatile gd_t *pgd = gd;
	int ret;
	u32 boot, bstage;

	ret = g_rom_api->query_boot_infor(QUERY_BT_DEV, &boot,
					  ((uintptr_t)&boot) ^ QUERY_BT_DEV);
	ret |= g_rom_api->query_boot_infor(QUERY_BT_STAGE, &bstage,
					   ((uintptr_t)&bstage) ^ QUERY_BT_STAGE);
	set_gd(pgd);

	if (ret != ROM_API_OKAY) {
		puts("ROMAPI: query_boot_infor() failed\n");
		return -1;
	}

	printf("Boot Stage: ");

	switch (bstage) {
	case BT_STAGE_PRIMARY:
		printf("Primary boot\n");
		break;
	case BT_STAGE_SECONDARY:
		printf("Secondary boot\n");
		break;
	case BT_STAGE_RECOVERY:
		printf("Recovery boot\n");
		break;
	case BT_STAGE_USB:
		printf("USB boot\n");
		break;
	default:
		printf("Unknown (0x%x)\n", bstage);
	}

	if (is_boot_from_stream_device(boot))
		return spl_romapi_load_image_stream(spl_image, bootdev);

	return spl_romapi_load_image_seekable(spl_image, bootdev, boot);
}
