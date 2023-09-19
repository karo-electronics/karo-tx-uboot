/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2015 Freescale Semiconductor, Inc.
 */
#include <linux/bitops.h>

struct watchdog_regs {
	u16	wcr;	/* Control */
	u16	wsr;	/* Service */
	u16	wrsr;	/* Reset Status */
};

#define WCR_WDZST	BIT(0)
#define WCR_WDBG	BIT(1)
#define WCR_WDE		BIT(2)
#define WCR_WDT		BIT(3)
#define WCR_SRS		BIT(4)
#define WCR_WDA		BIT(5)
#define SET_WCR_WT(x)	((x) << 8)
#define WCR_WT_MSK	SET_WCR_WT(0xFF)

#define WRSR_SFTW	BIT(0)
#define WRSR_TOUT	BIT(1)
