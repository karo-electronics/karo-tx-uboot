// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Lothar Waßmann <LW@KARO-electronics.de>
 * based on: board/freescale/imx93_evk/spl.c
 *           Copyright 2022 NXP
 */

#include <common.h>
#include <command.h>
#include <cpu_func.h>
#include <hang.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <spl.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/imx93_pins.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch-mx7ulp/gpio.h>
#include <asm/mach-imx/syscounter.h>
#include <asm/mach-imx/s400_api.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <linux/delay.h>
#include <asm/arch/clock.h>
#include <asm/arch/ccm_regs.h>
#include <asm/arch/ddr.h>
#include <power/pmic.h>
#include <power/pca9450.h>
#include <asm/arch/trdc.h>

#include <hexdump.h>

DECLARE_GLOBAL_DATA_PTR;

void __noreturn jump_to_image_no_args(struct spl_image_info *spl_image)
{
	typedef void __noreturn (*image_entry_noargs_t)(void);
	u32 __maybe_unused offset;
	const void *fdt_addr = gd->fdt_blob;

	image_entry_noargs_t image_entry =
		(image_entry_noargs_t)(unsigned long)spl_image->entry_point;

	debug("image entry point: 0x%p\n", image_entry);
debug("fdtaddr=%p flags=%08x\n", fdt_addr, spl_image->flags);
debug("BL31: %08lx..%08lx\n",
      spl_image->entry_point, spl_image->entry_point + spl_image->size - 1);

#if CONFIG_IS_ENABLED(IMX_HAB)
	/*
	 * HAB looks for the CSF at the end of the authenticated
	 * data, therefore we need to subtract the size of the
	 * CSF from the actual filesize
	 */
	offset = spl_image->size - CONFIG_CSF_SIZE;
	if (!imx_hab_authenticate_image(spl_image->load_addr,
					offset + IVT_SIZE +
					CSF_PAD_SIZE, offset)) {
		asm("\tmov x1, %0\n"
		    :: "r"(fdt_addr) : "x0", "x1", "x2", "x3");
		image_entry();
	} else {
		panic("spl: ERROR: failed to authenticate bootloader image\n");
	}
#endif
	asm("\tmov x1, %0\n"
	    :: "r"(fdt_addr) : "x0", "x1", "x2", "x3");
	image_entry();
}

int spl_board_boot_device(enum boot_device boot_dev_spl)
{
	return BOOT_DEVICE_BOOTROM;
}

void spl_board_init(void)
{
	puts("Normal Boot\n");
}

void spl_dram_init(void)
{
	int loops = 0;
	int max_loops = 5;

	while (ddr_init(&dram_timing)) {
		if (loops == 0)
			printf("Retrying...\n");
		if (loops < max_loops)
			loops++;
		else
			panic("DDR Training FAILED\n");
	}
	if (loops)
		printf("DDR Training OK after %u loops\n", loops);
}

static inline void __pmic_reg_write(struct udevice *dev, unsigned int reg,
				    u8 val, const char *fn, int ln)
{
	int ret;

	ret = pmic_reg_read(dev, reg);
	if (ret < 0) {
		printf("%s@%d: Failed to read pmic reg %02x: %d\n", fn, ln, reg, ret);
		return;
	}

	if (ret == val) {
		debug("%s@%d: Leaving pmic reg %02x at %02x\n", fn, ln, reg, ret);
		return;
	}
	debug("%s@%d: Changing pmic reg %02x from %02x to %02x\n", fn, ln, reg, ret, val);

	ret = pmic_reg_write(dev, reg, val);
	if (ret == 0)
		debug("%s@%d: wrote %02x to pmic reg %02x\n", fn, ln, val, reg);
	else
		printf("%s@%d: failed to write %02x to pmic reg %02x: %d\n", fn, ln,
		       val, reg, ret);
}

#define pmic_reg_write(d, r, v)	__pmic_reg_write(d, r, v, __func__, __LINE__)

#if CONFIG_IS_ENABLED(DM_PMIC_PCA9450)
int power_init_board(void)
{
	struct udevice *dev;
	int ret;

	ret = pmic_get("pmic@25", &dev);
	if (ret == -ENODEV) {
		puts("No pca9450@25\n");
		return 0;
	}
	if (ret != 0)
		return ret;

	if (IS_ENABLED(CONFIG_IMX9_LOW_DRIVE_MODE)) {
		/* 0.75v for Low drive mode */
		pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS0, 0x0c);
		pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS1, 0x0c);
		pmic_reg_write(dev, PCA9450_BUCK3OUT_DVS0, 0x0c);
		pmic_reg_write(dev, PCA9450_BUCK3OUT_DVS1, 0x0c);
	} else {
		/* 0.9v for Overdrive mode */
		pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS0, 0x18);
		pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS1, 0x18);
		pmic_reg_write(dev, PCA9450_BUCK3OUT_DVS0, 0x18);
		pmic_reg_write(dev, PCA9450_BUCK3OUT_DVS1, 0x18);
	}
	pmic_reg_write(dev, PCA9450_BUCK4OUT, 0x6c);
	pmic_reg_write(dev, PCA9450_BUCK5OUT, 0x30);
	pmic_reg_write(dev, PCA9450_BUCK6OUT, 0x14);

	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	pmic_reg_write(dev, PCA9450_BUCK123_DVS, 0x29);

	/* enable DVS control through PMIC_STBY_REQ */
	pmic_reg_write(dev, PCA9450_BUCK1CTRL, 0x59);
	pmic_reg_write(dev, PCA9450_BUCK3CTRL, 0x59);
	/* disable BUCK2 and activate discharge resistor */
	pmic_reg_write(dev, PCA9450_BUCK2CTRL, 0x48);
	pmic_reg_write(dev, PCA9450_BUCK4CTRL, 0x09);
	pmic_reg_write(dev, PCA9450_BUCK5CTRL, 0x09);
	pmic_reg_write(dev, PCA9450_BUCK6CTRL, 0x09);

	pmic_reg_write(dev, PCA9450_LDO_AD_CTRL, 0xf8);
	pmic_reg_write(dev, PCA9450_LDO1CTRL, 0xc2);
	pmic_reg_write(dev, PCA9450_LDO2CTRL, 0x00);
	pmic_reg_write(dev, PCA9450_LDO3CTRL, 0x0a);
	pmic_reg_write(dev, PCA9450_LDO4CTRL, 0x40);
	pmic_reg_write(dev, PCA9450_LDO5CTRL_L, 0x0f);
	pmic_reg_write(dev, PCA9450_LDO5CTRL_H, 0x00);

	/* disable I2C Level Translator */
	pmic_reg_write(dev, PCA9450_CONFIG2, 0x0);

	/* set WDOG_B_CFG to cold reset */
	pmic_reg_write(dev, PCA9450_RESET_CTRL, 0xA1);
	return 0;
}
#endif

void board_init_f(ulong dummy)
{
	int ret;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	timer_init();

	arch_cpu_init();

	board_early_init_f();

	spl_early_init();

	preloader_console_init();

	ret = arch_cpu_init_dm();
	if (ret) {
		printf("Failed to init Sentinel API: %d\n", ret);
	} else {
		printf("SOC: 0x%x\n", gd->arch.soc_rev);
		printf("LC: 0x%x\n", gd->arch.lifecycle);
	}

	power_init_board();

	if (!IS_ENABLED(CONFIG_IMX9_LOW_DRIVE_MODE))
		set_arm_core_max_clk();

	/* Init power of mix */
	soc_power_init();

	/* Setup TRDC for DDR access */
	trdc_init();

	/* DDR initialization */
	spl_dram_init();

	/* Put M33 into CPUWAIT for following kick */
	ret = m33_prepare();
	if (!ret)
		printf("M33 prepare ok\n");

	board_init_r(NULL, 0);
}
