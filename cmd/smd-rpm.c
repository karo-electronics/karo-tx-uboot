/*
 * (C) Copyright 2016 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <cli.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <power/pmic-qcom-smd-rpm.h>

DECLARE_GLOBAL_DATA_PTR;

static bool smd_inited;

static uint32_t smd_rpm_msg[] = {
	LDOA_RES_TYPE, 0, KEY_SOFTWARE_ENABLE, 4, 0,
	KEY_MICRO_VOLT, 4, 0,
	KEY_CURRENT, 4, 0,
};

#define smd_param_ldo		smd_rpm_msg[1]
#define smd_param_enable	smd_rpm_msg[4]
#define smd_param_uV		smd_rpm_msg[7]
#define smd_param_uA		smd_rpm_msg[10]

static int do_smd_init(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	if (!smd_inited) {
		smd_rpm_init();
		smd_inited = true;
	}
	return CMD_RET_SUCCESS;
}

static int do_smd_uninit(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	if (smd_inited) {
		smd_rpm_uninit();
		smd_inited = false;
	}
	return CMD_RET_SUCCESS;
}

#ifdef DEBUG
static void dump_rpm_msg(void *_msg, size_t msglen)
{
	uint32_t *msg = _msg;
	size_t m;

	printf("MSG:");
	for (m = 0; m < msglen / 4; m++) {
		printf(" %08x", msg[m]);
	}
	printf("\n");
}
#else
static inline void dump_rpm_msg(void *_msg, size_t msglen)
{
}
#endif

static int do_raw_write(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	size_t msglen = argc * sizeof(uint32_t);
	static uint32_t *msg;
	static size_t msgsize;
	int i;

	if (argc < 1)
		return CMD_RET_USAGE;

	if (msg == NULL || msgsize < msglen) {
		free(msg);
		msg = malloc(msglen);
		if (msg == NULL) {
			msgsize = 0;
			return CMD_RET_FAILURE;
		}
		msgsize = msglen;
	}

	for (i = 0; i < argc; i++)
		msg[i] = strtoul(argv[i], NULL, 16);

	dump_rpm_msg(msg, msglen);

	if (!smd_inited)
		smd_rpm_init();

	ret = rpm_send_data(msg, msglen, RPM_REQUEST_TYPE);
	if (ret)
		printf("Failed to configure regulator LDO%u\n", smd_param_ldo);

	if (!smd_inited)
		smd_rpm_uninit();

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int do_smd_write(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	uint ldo, uV, uA;
	int ret;
	size_t msglen;

	if (argv[0][0] == 'w')
		return do_raw_write(cmdtp, flag, argc - 1, &argv[1]);

	if (argc < 2 || argc > 4)
		return cmd_usage(cmdtp);

	msglen = 5 * sizeof(uint32_t);
	smd_param_enable = argv[0][0] == 'e';

	ldo = simple_strtoul(argv[1], NULL, 10);
	if (ldo > 18)
		return CMD_RET_USAGE;

	smd_param_ldo = ldo;

	if (argc > 2) {
		if (ldo > 3) {
			uV = simple_strtoul(argv[2], NULL, 10);
			smd_param_uV = uV;
			msglen += 3 * sizeof(uint32_t);
		} else {
			printf("LDO%u has no voltage control\n", ldo);
		}
	}
	if (argc > 3) {
		uA = simple_strtoul(argv[3], NULL, 10);
		smd_param_uA = uA;
		msglen += 3 * sizeof(uint32_t);
	}
	printf("%sabling LDO%u", smd_param_enable == GENERIC_ENABLE ? "En" : "Dis",
		smd_param_ldo);
	if (argc > 2)
		printf(": %u.%03uV", smd_param_uV / 1000000,
			smd_param_uV / 1000 % 1000);
	if (argc > 3)
		printf(", %u.%03umA", smd_param_uA / 1000,
			smd_param_uA % 1000);
	printf("\n");

	if (!smd_inited)
		smd_rpm_init();

	dump_rpm_msg(smd_rpm_msg, msglen);
	ret = rpm_send_data(smd_rpm_msg, msglen, RPM_REQUEST_TYPE);
	if (ret)
		printf("Failed to configure regulator LDO%u\n", smd_param_ldo);

	if (!smd_inited)
		smd_rpm_uninit();

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_smd_sub[] = {
	U_BOOT_CMD_MKENT(init, 0, 1, do_smd_init, "", ""),
	U_BOOT_CMD_MKENT(uninit, 0, 1, do_smd_uninit, "", ""),
	U_BOOT_CMD_MKENT(enable, 4, 1, do_smd_write, "", ""),
	U_BOOT_CMD_MKENT(disable, 5, 0, do_smd_write, "", ""),
	U_BOOT_CMD_MKENT(write, 4, 1, do_smd_write, "", ""),
};

static inline void smd_reloc(void)
{
	static int relocated;

	if (!relocated) {
		fixup_cmdtable(cmd_smd_sub, ARRAY_SIZE(cmd_smd_sub));
		relocated = 1;
	};
}

/**
 * do_smd() - Handle the "smd" command-line command
 * @cmdtp:	Command data struct pointer
 * @flag:	Command flag
 * @argc:	Command-line argument count
 * @argv:	Array of command-line arguments
 *
 * Returns zero on success, CMD_RET_USAGE in case of misuse and negative
 * on error.
 */
static int do_smd(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	cmd_tbl_t *c;

#ifdef CONFIG_NEEDS_MANUAL_RELOC
	smd_reloc();
#endif

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'smd' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], cmd_smd_sub, ARRAY_SIZE(cmd_smd_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		printf("subcommand '%s' not found\n", argv[0]);
	return CMD_RET_USAGE;
}

#ifdef CONFIG_SYS_LONGHELP
static char smd_help_text[] =
	"\tsmd init\n"
	"\tsmd uninit\n"
	"\tsmd enable <ldo#> [<uV>] [<uA>]    - Enable LDO<ldo#> optionally setting voltage and current\n"
	"\tsmd disable <ldo#> [<uV>] [<uA>]   - Disable LDO<ldö#>\n"
	"\tsmd write <keycode> <key> [<param> <value>] [...]\n"
	"\tsmd read <len>\n"
	;
#endif

U_BOOT_CMD(
	smd, 16, 0, do_smd,
	"Qualcomm SMD sub-system",
	smd_help_text
);
