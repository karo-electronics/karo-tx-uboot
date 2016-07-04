/*
 * Freescale i.MX28 GPIO control code
 *
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <netdev.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>

#if	defined(CONFIG_SOC_MX23)
#define	PINCTRL_BANKS		3
#define	PINCTRL_DOUT(n)		(0x0500 + ((n) * 0x10))
#define	PINCTRL_DIN(n)		(0x0600 + ((n) * 0x10))
#define	PINCTRL_DOE(n)		(0x0700 + ((n) * 0x10))
#define	PINCTRL_PIN2IRQ(n)	(0x0800 + ((n) * 0x10))
#define	PINCTRL_IRQEN(n)	(0x0900 + ((n) * 0x10))
#define	PINCTRL_IRQSTAT(n)	(0x0c00 + ((n) * 0x10))
#elif	defined(CONFIG_SOC_MX28)
#define	PINCTRL_BANKS		5
#define	PINCTRL_DOUT(n)		(0x0700 + ((n) * 0x10))
#define	PINCTRL_DIN(n)		(0x0900 + ((n) * 0x10))
#define	PINCTRL_DOE(n)		(0x0b00 + ((n) * 0x10))
#define	PINCTRL_PIN2IRQ(n)	(0x1000 + ((n) * 0x10))
#define	PINCTRL_IRQEN(n)	(0x1100 + ((n) * 0x10))
#define	PINCTRL_IRQSTAT(n)	(0x1400 + ((n) * 0x10))
#else
#error "Please select CONFIG_SOC_MX23 or CONFIG_SOC_MX28"
#endif

void mxs_gpio_init(void)
{
	int i;

	for (i = 0; i < PINCTRL_BANKS; i++) {
		writel(0, MXS_PINCTRL_BASE + PINCTRL_PIN2IRQ(i));
		writel(0, MXS_PINCTRL_BASE + PINCTRL_IRQEN(i));
		/* Use SCT address here to clear the IRQSTAT bits */
		writel(0xffffffff, MXS_PINCTRL_BASE + PINCTRL_IRQSTAT(i) + 8);
	}
}

int gpio_get_value(unsigned gpio)
{
	uint32_t bank = MXS_GPIO_TO_BANK(gpio);
	uint32_t offset = PINCTRL_DIN(bank);
	struct mxs_register_32 *reg = (void *)(MXS_PINCTRL_BASE + offset);

	if (bank >= PINCTRL_BANKS)
		return -EINVAL;

	return (readl(&reg->reg) >> MXS_GPIO_TO_PIN(gpio)) & 1;
}

int gpio_set_value(unsigned gpio, int value)
{
	uint32_t bank = MXS_GPIO_TO_BANK(gpio);
	uint32_t offset = PINCTRL_DOUT(bank);
	struct mxs_register_32 *reg = (void *)(MXS_PINCTRL_BASE + offset);

	if (bank >= PINCTRL_BANKS)
		return -EINVAL;

	if (value)
		writel(1 << MXS_GPIO_TO_PIN(gpio), &reg->reg_set);
	else
		writel(1 << MXS_GPIO_TO_PIN(gpio), &reg->reg_clr);

	return 0;
}

int gpio_direction_input(unsigned gpio)
{
	uint32_t bank = MXS_GPIO_TO_BANK(gpio);
	uint32_t offset = PINCTRL_DOE(bank);
	struct mxs_register_32 *reg = (void *)(MXS_PINCTRL_BASE + offset);

	if (bank >= PINCTRL_BANKS)
		return -EINVAL;

	writel(1 << MXS_GPIO_TO_PIN(gpio), &reg->reg_clr);

	return 0;
}

int gpio_direction_output(unsigned gpio, int value)
{
	uint32_t bank = MXS_GPIO_TO_BANK(gpio);
	uint32_t offset = PINCTRL_DOE(bank);
	struct mxs_register_32 *reg = (void *)(MXS_PINCTRL_BASE + offset);

	if (bank >= PINCTRL_BANKS)
		return -EINVAL;

	gpio_set_value(gpio, value);

	writel(1 << MXS_GPIO_TO_PIN(gpio), &reg->reg_set);

	return 0;
}

int gpio_request(unsigned gpio, const char *label)
{
	if (MXS_GPIO_TO_BANK(gpio) >= PINCTRL_BANKS) {
		printf("%s(): Invalid GPIO%d (GPIO_%u_%u) requested; possibly intended: GPIO_%u_%u\n",
			__func__, gpio, gpio / 32, gpio % 32,
			PAD_BANK(gpio), PAD_PIN(gpio));
		printf("Linear GPIO number required rather than iomux_cfg_t cookie!\n");
		printf("Possibly missing MXS_PAD_TO_GPIO() in the GPIO specification.\n");
		return -EINVAL;
	}

	return 0;
}

int gpio_free(unsigned gpio)
{
	return 0;
}
