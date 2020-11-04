// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2020 Lothar Waßmann <LW@KARO-electronics.de>
 */

#include <config.h>
#include <common.h>
#include <env.h>
#include <dm/uclass.h>
#include <linux/mtd/mtd.h>

void board_mtdparts_default(const char **mtdids, const char **mtdparts)
{
	struct udevice *dev;
	char s_nand0[512];
	static char parts[512];
	static char ids[128];

	if (parts[0] && ids[0]) {
		*mtdids = ids;
		*mtdparts = parts;
		debug("%s@%d: reusing mtdids=%s & mtdparts=%s\n",
		      __func__, __LINE__, ids, parts);
		return;
	}

	parts[0] = '\0';
	ids[0] = '\0';

	if (uclass_get_device(UCLASS_MTD, 0, &dev))
		return;

	env_get_f("mtdparts_nand0", s_nand0, sizeof(s_nand0));

	snprintf(ids, sizeof(ids), "spi-nand0=spi-nand0");
	snprintf(parts, sizeof(parts), "mtdparts=spi-nand0:%s",
		 s_nand0);

	debug("%s@%d: mtdids=%s & mtdparts=%s\n", __func__, __LINE__,
	      ids, parts);

	*mtdids = ids;
	*mtdparts = parts;
}
