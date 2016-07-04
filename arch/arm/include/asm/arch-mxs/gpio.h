/*
 * Freescale i.MX28 GPIO
 *
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef	__MX28_GPIO_H__
#define	__MX28_GPIO_H__

#ifdef	CONFIG_MXS_GPIO
void mxs_gpio_init(void);
#else
inline void mxs_gpio_init(void) {}
#endif

#define MXS_GPIO_NR(p, o)	(((p) * 32) | ((o) & 0x1f))
#define MXS_GPIO_TO_BANK(gpio)	((gpio) / 32)
#define MXS_GPIO_TO_PIN(gpio)	((gpio) % 32)

#endif	/* __MX28_GPIO_H__ */
