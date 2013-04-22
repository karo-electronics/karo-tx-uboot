/*
 * clocks_am33xx.h
 *
 * AM33xx clock define
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CLOCKS_AM33XX_H_
#define _CLOCKS_AM33XX_H_

#define OSC	(V_OSCK/1000000)

#ifndef CONFIG_SYS_MPU_CLK
/* MAIN PLL Fdll = 550 MHZ, */
#define MPUPLL_M	550
#else
#define MPUPLL_M	CONFIG_SYS_MPU_CLK
#endif
#define MPUPLL_N	(OSC - 1)
#define MPUPLL_M2	1

/* Core PLL Fdll = 1 GHZ, */
#define COREPLL_M	1000
#define COREPLL_N	(OSC - 1)

#define COREPLL_M4	10	/* CORE_CLKOUTM4 = 200 MHZ */
#define COREPLL_M5	8	/* CORE_CLKOUTM5 = 250 MHZ */
#define COREPLL_M6	4	/* CORE_CLKOUTM6 = 500 MHZ */

/*
 * USB PHY clock is 960 MHZ. Since, this comes directly from Fdll, Fdll
 * frequency needs to be set to 960 MHZ. Hence,
 * For clkout = 192 MHZ, Fdll = 960 MHZ, divider values are given below
 */
#define PERPLL_M	960
#define PERPLL_N	(OSC - 1)
#define PERPLL_M2	5

/* DDR Freq is 266 MHZ for now */
/* Set Fdll = 400 MHZ , Fdll = M * 2 * CLKINP/ N + 1; clkout = Fdll /(2 * M2) */
#ifndef CONFIG_SYS_DDR_CLK
#define DDRPLL_M	266
#else
#define DDRPLL_M	CONFIG_SYS_DDR_CLK
#endif
#define DDRPLL_N	(OSC - 1)
#define DDRPLL_M2	1

#define DISPPLL_M	200
#define DISPPLL_N	(OSC - 1)
#define DISPPLL_M2	1

extern void pll_init(void);
extern void enable_emif_clocks(void);

#endif	/* endif _CLOCKS_AM33XX_H_ */
