/*
 * (C) Copyright 2017 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * based on: cmd/i2c.c
 *   (C) Copyright 2009
 *   Sergey Kubushyn, himself, ksi@koi8.net
 *
 *   (C) Copyright 2001
 *   Gerald Van Baren, Custom IDEAS, vanbaren@cideas.com.
 */

#include <common.h>
#include <cli.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <environment.h>
#include <errno.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <linux/compiler.h>
#include <spmi/spmi.h>

DECLARE_GLOBAL_DATA_PTR;

enum spmi_err_op {
	SPMI_ERR_READ,
	SPMI_ERR_WRITE,
};

static int spmi_report_err(int ret, enum spmi_err_op op)
{
	printf("Error %s the chip: %d\n",
	       op == SPMI_ERR_READ ? "reading" : "writing", ret);

	return CMD_RET_FAILURE;
}

/**
 * do_spmi_read() - Handle the "spmi read" command-line command
 * @cmdtp:	Command data struct pointer
 * @flag:	Command flag
 * @argc:	Command-line argument count
 * @argv:	Array of command-line arguments
 *
 * Returns zero on success, CMD_RET_USAGE in case of misuse and negative
 * on error.
 *
 * Syntax:
 *	spmi read {spmi_chip} {devaddr}{.0, .1, .2} {len} {memaddr}
 */
static int do_spmi_read(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	uint sid, pid, reg;
	int ret;
	struct udevice *dev;

	if (argc != 4)
		return cmd_usage(cmdtp);

	ret = uclass_get_device_by_name(UCLASS_SPMI, "spmi", &dev);
	if (ret) {
		printf("Failed to get SPMI bus: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	/*
	 * SPMI chip address
	 */
	sid = simple_strtoul(argv[1], NULL, 16);

	/*
	 * SPMI data address within the chip.  This can be 1 or
	 * 2 bytes long.  Some day it might be 3 bytes long :-).
	 */
	pid = simple_strtoul(argv[2], NULL, 16);
	reg = simple_strtoul(argv[3], NULL, 16);

	ret = spmi_reg_read(dev, sid, pid, reg);
	if (ret < 0)
		return spmi_report_err(ret, SPMI_ERR_READ);

	printf("%02x\n", ret);
	return CMD_RET_SUCCESS;
}

static int do_spmi_write(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	uint sid, pid, reg;
	uint val;
	int ret;
	struct udevice *dev;

	if (argc != 5)
		return cmd_usage(cmdtp);

	ret = uclass_get_device_by_name(UCLASS_SPMI, "spmi", &dev);
	if (ret) {
		printf("Failed to get SPMI bus: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	/*
	 * SPMI chip address
	 */
	sid = simple_strtoul(argv[1], NULL, 16);
	pid = simple_strtoul(argv[2], NULL, 16);
	reg = simple_strtoul(argv[3], NULL, 16);
	val = simple_strtoul(argv[4], NULL, 16);
	if (val > 255) {
		printf("Value %08x out of range [0..255]\n", val);
		return CMD_RET_FAILURE;
	}

	ret = spmi_reg_write(dev, sid, pid, reg, val);
	if (ret)
		return spmi_report_err(ret, SPMI_ERR_WRITE);

	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_spmi_sub[] = {
	U_BOOT_CMD_MKENT(read, 4, 1, do_spmi_read, "", ""),
	U_BOOT_CMD_MKENT(write, 5, 0, do_spmi_write, "", ""),
};

static __maybe_unused void spmi_reloc(void)
{
	static int relocated;

	if (!relocated) {
		fixup_cmdtable(cmd_spmi_sub, ARRAY_SIZE(cmd_spmi_sub));
		relocated = 1;
	};
}

/**
 * do_spmi() - Handle the "spmi" command-line command
 * @cmdtp:	Command data struct pointer
 * @flag:	Command flag
 * @argc:	Command-line argument count
 * @argv:	Array of command-line arguments
 *
 * Returns zero on success, CMD_RET_USAGE in case of misuse and negative
 * on error.
 */
static int do_spmi(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	cmd_tbl_t *c;

#ifdef CONFIG_NEEDS_MANUAL_RELOC
	spmi_reloc();
#endif

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'spmi' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], cmd_spmi_sub, ARRAY_SIZE(cmd_spmi_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		printf("subcommand '%s' not found\n", argv[0]);
	return CMD_RET_USAGE;
}

#ifdef CONFIG_SYS_LONGHELP
static char spmi_help_text[] =
	"\tspmi read <sid> <pid> <reg>         - read register <reg> of peripheral <pid> on slave <sid>\n"
	"\tspmi write <sid> <pid> <reg> <val>  - write <val> to register <reg> of peripheral <pid> on slave <sid>\n"
	;
#endif

U_BOOT_CMD(
	spmi, 6, 1, do_spmi,
	"SPMI sub-system",
	spmi_help_text
);
