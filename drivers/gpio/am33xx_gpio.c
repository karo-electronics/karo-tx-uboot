/*
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/sizes.h>
#include <asm/arch/hardware.h>
#include <asm/arch/gpio.h>

struct gpio_regs {
	unsigned int res1[0x134 / 4];
	unsigned int oe;		/* 0x134 */
	unsigned int datain;		/* 0x138 */
	unsigned int res2[0x54 / 4];
	unsigned int cleardataout;	/* 0x190 */
	unsigned int setdataout;	/* 0x194 */
};

static const struct gpio_regs *gpio_base[] = {
	(struct gpio_regs *)AM33XX_GPIO0_BASE,
	(struct gpio_regs *)AM33XX_GPIO1_BASE,
	(struct gpio_regs *)AM33XX_GPIO2_BASE,
	(struct gpio_regs *)AM33XX_GPIO3_BASE,
};

static unsigned long gpio_map[ARRAY_SIZE(gpio_base)] __attribute__((section("data")));

#define MAX_GPIO	(ARRAY_SIZE(gpio_base) * 32)

int gpio_request(unsigned gpio, const char *name)
{
	if (gpio >= MAX_GPIO)
		return -EINVAL;
	if (test_and_set_bit(gpio, gpio_map))
		return -EBUSY;
	return 0;
}

int gpio_free(unsigned gpio)
{
	if (gpio >= MAX_GPIO)
		return -EINVAL;

	if (test_bit(gpio, gpio_map))
		__clear_bit(gpio, gpio_map);
	else
		printf("ERROR: trying to free unclaimed GPIO %u\n", gpio);

	return 0;
}

int gpio_set_value(unsigned gpio, int val)
{
	int bank = gpio / 32;
	int mask = 1 << (gpio % 32);

	if (bank >= ARRAY_SIZE(gpio_base))
		return -EINVAL;

	if (val)
		writel(mask, &gpio_base[bank]->setdataout);
	else
		writel(mask, &gpio_base[bank]->cleardataout);
	return 0;
}

int gpio_direction_input(unsigned gpio)
{
	int bank = gpio / 32;
	int mask = 1 << (gpio % 32);

	if (bank >= ARRAY_SIZE(gpio_base))
		return -EINVAL;

	writel(readl(&gpio_base[bank]->oe) | mask, &gpio_base[bank]->oe);
	return 0;
}

int gpio_direction_output(unsigned gpio, int val)
{
	int bank = gpio / 32;
	int mask = 1 << (gpio % 32);

	if (bank >= ARRAY_SIZE(gpio_base))
		return -EINVAL;

	gpio_set_value(gpio, val);
	writel(readl(&gpio_base[bank]->oe) & ~mask, &gpio_base[bank]->oe);
	return 0;
}
