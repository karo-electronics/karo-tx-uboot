/*
 * (C) Copyright 2014 Lothar Waßmann <LW@KARO-electronics.de>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <errno.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <environment.h>

#include "karo.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_UBOOT_IGNORE_ENV
void env_cleanup(void)
{
	set_default_env(NULL);
}
#else
static const char const *cleanup_vars[] = {
	"bootargs",
	"fileaddr",
	"filesize",
	"safeboot",
	"wdreset",
};

void env_cleanup(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cleanup_vars); i++) {
		setenv(cleanup_vars[i], NULL);
	}
}
#endif
