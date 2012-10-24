/*
 * Copyright (c) 2012 Lothar Waßmann <LW@KARO-electronics.de>
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
#ifndef _GPIO_AM33XX_H
#define _GPIO_AM33XX_H

#include <asm-generic/gpio.h>

#define AM33XX_GPIO_NR(bank, pin)	(((bank) << 5) | (pin))

#endif /* _GPIO_AM33XX_H */
