/*
 * Copyright (C) 2010-2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hab.h>

HAB_FUNC(entry, hab_status_t)
HAB_FUNC(exit, hab_status_t)
HAB_FUNC5(authenticate_image, void *, uint8_t, size_t, void **, size_t *, hab_loader_callback_f_t)
//HAB_FUNC1(run_dcd, hab_status_t, const uint8_t *)
HAB_FUNC2(run_csf, hab_status_t, const uint8_t *, uint8_t)
HAB_FUNC2(report_status, hab_status_t, hab_config_t *, hab_state_t *)
HAB_FUNC4(report_event, hab_status_t, hab_status_t, uint32_t, uint8_t *, size_t *)
HAB_FUNC3(check_target, hab_status_t, uint8_t, const void *, size_t)
HAB_FUNC3(assert, hab_status_t, uint8_t, const void *, size_t)

struct mx6_ivt {
	u32 header;
	u32 entry;
	u32 rsrvd1;
	void *dcd;
	struct mx6_boot_data *boot_data;
	void *self;
	void *csf;
	u32 rsrvd2;
};

struct mx6_boot_data {
	void *start;
	u32 length;
	u32 plugin;
};

#define IVT_SIZE		0x20
#define ALIGN_SIZE		0x400
#define CSF_PAD_SIZE		0x2000
#define MX6DQ_PU_IROM_MMU_EN_VAR	0x009024a8
#define MX6DLS_PU_IROM_MMU_EN_VAR	0x00901dd0
#define MX6SL_PU_IROM_MMU_EN_VAR	0x00900a18

/*
 * +------------+  0x0 (DDR_UIMAGE_START) -
 * |   Header   |                          |
 * +------------+  0x40                    |
 * |            |                          |
 * |            |                          |
 * |            |                          |
 * |            |                          |
 * | Image Data |                          |
 * .            |                          |
 * .            |                           > Stuff to be authenticated ----+
 * .            |                          |                                |
 * |            |                          |                                |
 * |            |                          |                                |
 * +------------+                          |                                |
 * |            |                          |                                |
 * | Fill Data  |                          |                                |
 * |            |                          |                                |
 * +------------+ Align to ALIGN_SIZE      |                                |
 * |    IVT     |                          |                                |
 * +------------+ + IVT_SIZE              -                                 |
 * |            |                                                           |
 * |  CSF DATA  | <---------------------------------------------------------+
 * |            |
 * +------------+
 * |            |
 * | Fill Data  |
 * |            |
 * +------------+ + CSF_PAD_SIZE
 */

#define MAX_RECORD_BYTES     (8*1024) /* 4 kbytes */

struct record {
	uint8_t  tag;						/* Tag */
	uint8_t  len[2];					/* Length */
	uint8_t  par;						/* Version */
	uint8_t  contents[MAX_RECORD_BYTES];/* Record Data */
	bool	 any_rec_flag;
};

char *rsn_str[] = {"RSN = HAB_RSN_ANY (0x00)\n",
				   "RSN = HAB_ENG_FAIL (0x30)\n",
				   "RSN = HAB_INV_ADDRESS (0x22)\n",
				   "RSN = HAB_INV_ASSERTION (0x0C)\n",
				   "RSN = HAB_INV_CALL (0x28)\n",
				   "RSN = HAB_INV_CERTIFICATE (0x21)\n",
				   "RSN = HAB_INV_COMMAND (0x06)\n",
				   "RSN = HAB_INV_CSF (0x11)\n",
				   "RSN = HAB_INV_DCD (0x27)\n",
				   "RSN = HAB_INV_INDEX (0x0F)\n",
				   "RSN = HAB_INV_IVT (0x05)\n",
				   "RSN = HAB_INV_KEY (0x1D)\n",
				   "RSN = HAB_INV_RETURN (0x1E)\n",
				   "RSN = HAB_INV_SIGNATURE (0x18)\n",
				   "RSN = HAB_INV_SIZE (0x17)\n",
				   "RSN = HAB_MEM_FAIL (0x2E)\n",
				   "RSN = HAB_OVR_COUNT (0x2B)\n",
				   "RSN = HAB_OVR_STORAGE (0x2D)\n",
				   "RSN = HAB_UNS_ALGORITHM (0x12)\n",
				   "RSN = HAB_UNS_COMMAND (0x03)\n",
				   "RSN = HAB_UNS_ENGINE (0x0A)\n",
				   "RSN = HAB_UNS_ITEM (0x24)\n",
				   "RSN = HAB_UNS_KEY (0x1B)\n",
				   "RSN = HAB_UNS_PROTOCOL (0x14)\n",
				   "RSN = HAB_UNS_STATE (0x09)\n",
				   "RSN = INVALID\n",
				   NULL};

char *sts_str[] = {"STS = HAB_SUCCESS (0xF0)\n",
				   "STS = HAB_FAILURE (0x33)\n",
				   "STS = HAB_WARNING (0x69)\n",
				   "STS = INVALID\n",
				   NULL};

char *eng_str[] = {"ENG = HAB_ENG_ANY (0x00)\n",
				   "ENG = HAB_ENG_SCC (0x03)\n",
				   "ENG = HAB_ENG_RTIC (0x05)\n",
				   "ENG = HAB_ENG_SAHARA (0x06)\n",
				   "ENG = HAB_ENG_CSU (0x0A)\n",
				   "ENG = HAB_ENG_SRTC (0x0C)\n",
				   "ENG = HAB_ENG_DCP (0x1B)\n",
				   "ENG = HAB_ENG_CAAM (0x1D)\n",
				   "ENG = HAB_ENG_SNVS (0x1E)\n",
				   "ENG = HAB_ENG_OCOTP (0x21)\n",
				   "ENG = HAB_ENG_DTCP (0x22)\n",
				   "ENG = HAB_ENG_ROM (0x36)\n",
				   "ENG = HAB_ENG_HDCP (0x24)\n",
				   "ENG = HAB_ENG_RTL (0x77)\n",
				   "ENG = HAB_ENG_SW (0xFF)\n",
				   "ENG = INVALID\n",
				   NULL};

char *ctx_str[] = {"CTX = HAB_CTX_ANY(0x00)\n",
				   "CTX = HAB_CTX_FAB (0xFF)\n",
				   "CTX = HAB_CTX_ENTRY (0xE1)\n",
				   "CTX = HAB_CTX_TARGET (0x33)\n",
				   "CTX = HAB_CTX_AUTHENTICATE (0x0A)\n",
				   "CTX = HAB_CTX_DCD (0xDD)\n",
				   "CTX = HAB_CTX_CSF (0xCF)\n",
				   "CTX = HAB_CTX_COMMAND (0xC0)\n",
				   "CTX = HAB_CTX_AUT_DAT (0xDB)\n",
				   "CTX = HAB_CTX_ASSERT (0xA0)\n",
				   "CTX = HAB_CTX_EXIT (0xEE)\n",
				   "CTX = INVALID\n",
				   NULL};

uint8_t hab_statuses[5] = {
	HAB_STS_ANY,
	HAB_FAILURE,
	HAB_WARNING,
	HAB_SUCCESS,
	-1
};

uint8_t hab_reasons[26] = {
	HAB_RSN_ANY,
	HAB_ENG_FAIL,
	HAB_INV_ADDRESS,
	HAB_INV_ASSERTION,
	HAB_INV_CALL,
	HAB_INV_CERTIFICATE,
	HAB_INV_COMMAND,
	HAB_INV_CSF,
	HAB_INV_DCD,
	HAB_INV_INDEX,
	HAB_INV_IVT,
	HAB_INV_KEY,
	HAB_INV_RETURN,
	HAB_INV_SIGNATURE,
	HAB_INV_SIZE,
	HAB_MEM_FAIL,
	HAB_OVR_COUNT,
	HAB_OVR_STORAGE,
	HAB_UNS_ALGORITHM,
	HAB_UNS_COMMAND,
	HAB_UNS_ENGINE,
	HAB_UNS_ITEM,
	HAB_UNS_KEY,
	HAB_UNS_PROTOCOL,
	HAB_UNS_STATE,
	-1
};

uint8_t hab_contexts[12] = {
	HAB_CTX_ANY,
	HAB_CTX_FAB,
	HAB_CTX_ENTRY,
	HAB_CTX_TARGET,
	HAB_CTX_AUTHENTICATE,
	HAB_CTX_DCD,
	HAB_CTX_CSF,
	HAB_CTX_COMMAND,
	HAB_CTX_AUT_DAT,
	HAB_CTX_ASSERT,
	HAB_CTX_EXIT,
	-1
};

uint8_t hab_engines[16] = {
	HAB_ENG_ANY,
	HAB_ENG_SCC,
	HAB_ENG_RTIC,
	HAB_ENG_SAHARA,
	HAB_ENG_CSU,
	HAB_ENG_SRTC,
	HAB_ENG_DCP,
	HAB_ENG_CAAM,
	HAB_ENG_SNVS,
	HAB_ENG_OCOTP,
	HAB_ENG_DTCP,
	HAB_ENG_ROM,
	HAB_ENG_HDCP,
	HAB_ENG_RTL,
	HAB_ENG_SW,
	-1
};

bool is_hab_enabled(void)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank *bank = &ocotp->bank[0];
	struct fuse_bank0_regs *fuse =
		(struct fuse_bank0_regs *)bank->fuse_regs;
	uint32_t reg = readl(&fuse->cfg5);
	static int first = 1;

	if (first) {
		debug("rvt_base=%p\n", hab_rvt_base());
		debug("hab_rvt_entry=%p\n", hab_rvt_entry_p);
		debug("hab_rvt_exit=%p\n", hab_rvt_exit_p);
		debug("hab_rvt_check_target=%p\n", hab_rvt_check_target_p);
		debug("hab_rvt_authenticate_image=%p\n", hab_rvt_authenticate_image_p);
		debug("hab_rvt_report_event=%p\n", hab_rvt_report_event_p);
		debug("hab_rvt_report_status=%p\n", hab_rvt_report_status_p);
		debug("hab_rvt_assert=%p\n", hab_rvt_assert_p);
		first = 0;
	}
	return (reg & 0x2) == 0x2;
}

static inline uint8_t get_idx(uint8_t *list, uint8_t tgt)
{
	uint8_t idx = 0;
	uint8_t element = list[idx];
	while (element != -1) {
		if (element == tgt)
			return idx;
		element = list[++idx];
	}
	return -1;
}

void process_event_record(uint8_t *event_data, size_t bytes)
{
	struct record *rec = (struct record *)event_data;

	printf("\n\n%s", sts_str[get_idx(hab_statuses, rec->contents[0])]);
	printf("%s", rsn_str[get_idx(hab_reasons, rec->contents[1])]);
	printf("%s", ctx_str[get_idx(hab_contexts, rec->contents[2])]);
	printf("%s", eng_str[get_idx(hab_engines, rec->contents[3])]);
}

void display_event(uint8_t *event_data, size_t bytes)
{
	uint32_t i;

	if (!(event_data && bytes > 0))
		return;

	for (i = 0; i < bytes; i++) {
		if (i == 0)
			printf("\t0x%02x", event_data[i]);
		else if ((i % 8) == 0)
			printf("\n\t0x%02x", event_data[i]);
		else
			printf(" 0x%02x", event_data[i]);
	}

	process_event_record(event_data, bytes);
}

int get_hab_status(void)
{
	static uint32_t last_hab_event __attribute__((section(".data")));
	uint32_t index = last_hab_event; /* Loop index */
	uint8_t event_data[128]; /* Event data buffer */
	size_t bytes = sizeof(event_data); /* Event size in bytes */
	enum hab_config config;
	enum hab_state state;
	int ret;

	if (is_hab_enabled())
		puts("Secure boot enabled\n");
	else
		puts("Secure boot disabled\n");

	/* Check HAB status */
	config = state = 0; /* ROM code assumes short enums! */
	ret = hab_rvt_report_status(&config, &state);
	printf("HAB Configuration: 0x%02x, HAB State: 0x%02x\n",
		config, state);
	if (ret != HAB_SUCCESS) {
		/* Display HAB Error events */
		while (hab_rvt_report_event(HAB_STS_ANY, index, event_data,
					&bytes) == HAB_SUCCESS) {
			puts("\n");
			printf("--------- HAB Event %d -----------------\n",
			       index + 1);
			puts("event data:\n");
			display_event(event_data, bytes);
			puts("\n");
			bytes = sizeof(event_data);
			index++;
		}
		ret = index - last_hab_event;
		last_hab_event = index;
	} else {
		/* Display message if no HAB events are found */
		puts("No HAB Events Found!\n");
		ret = 0;
	}
	return ret;
}

static inline hab_status_t hab_init(void)
{
	hab_status_t ret;

	if (!is_hab_enabled()) {
		puts("hab fuse not enabled\n");
		return HAB_FAILURE;
	}

	hab_caam_clock_enable(1);

	ret = hab_rvt_entry();
	debug("hab_rvt_entry() returned %02x\n", ret);
	if (ret != HAB_SUCCESS) {
		printf("hab entry function failed: %02x\n", ret);
		hab_caam_clock_enable(0);
	}

	return ret;
}

static inline hab_status_t hab_exit(void)
{
	hab_status_t ret;

	ret = hab_rvt_exit();
	if (ret != HAB_SUCCESS)
		printf("hab exit function failed: %02x\n", ret);

	hab_caam_clock_enable(0);

	return ret;
}

static hab_status_t hab_check_target(hab_target_t type, uint32_t addr, size_t len)
{
	hab_status_t ret;

	ret = hab_init();
	if (ret != HAB_SUCCESS)
		return ret;

	ret = hab_rvt_check_target(type, (void *)addr, len);
	if (ret != HAB_SUCCESS) {
		printf("check_target(0x%08x, 0x%08x) failed: %d\n",
			addr, len, ret);
		return ret;
	}
	ret = hab_exit();

	if (ret == HAB_SUCCESS && get_hab_status() > 0) {
		return HAB_FAILURE;
	}
	return ret;
}

static hab_status_t hab_assert(uint32_t type, uint32_t addr, size_t len)
{
	hab_status_t ret;

	ret = hab_init();
	if (ret != HAB_SUCCESS)
		return ret;

	ret = hab_rvt_assert(type, (void *)addr, len);
	if (ret != HAB_SUCCESS) {
		printf("assert(0x%08x, 0x%08x) failed: %d\n",
			addr, len, ret);
		return ret;
	}
	ret = hab_exit();

	if (ret == HAB_SUCCESS && get_hab_status() > 0) {
		return HAB_FAILURE;
	}
	return ret;
}

static int do_hab_status(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	if (argc != 1)
		return CMD_RET_USAGE;

	get_hab_status();

	return CMD_RET_SUCCESS;
}

static int do_hab_check_target(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	hab_target_t type = HAB_TGT_ANY;
	uint32_t addr;
	size_t len;

	if (argc < 3)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], NULL, 16);
	len = simple_strtoul(argv[2], NULL, 16);
	if (argc > 3) {
		switch (argv[3][0]) {
		case 'p':
		case 'P':
			type = HAB_TGT_PERIPHERAL;
			break;
		case 'm':
		case 'M':
			type = HAB_TGT_MEMORY;
			break;
		case 'a':
		case 'A':
			break;
		default:
			printf("Invalid type '%s'\n", argv[3]);
			return CMD_RET_USAGE;
		}
	}
	if (hab_check_target(type, addr, len) != HAB_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_hab_assert(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	uint32_t type = 0;
	uint32_t addr;
	size_t len;

	if (argc < 3)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], NULL, 16);
	len = simple_strtoul(argv[2], NULL, 16);
	if (argc > 3) {
		type = simple_strtoul(argv[3], NULL, 16);
	}

	if (hab_assert(type, addr, len) != HAB_SUCCESS)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
		hab_status, CONFIG_SYS_MAXARGS, 1, do_hab_status,
		"display HAB status",
		""
	  );

U_BOOT_CMD(
		hab_check_target, 4, 0, do_hab_check_target,
		"verify an address range via HAB",
		"addr len [type]\n"
		"\t\taddr -\taddress to verify\n"
		"\t\tlen -\tlength of addr range to verify\n"
	  );

U_BOOT_CMD(
		hab_assert, 4, 0, do_hab_assert,
		"Test an assertion against the HAB audit log",
		"addr len [type]\n"
		"\t\taddr -\taddress to verify\n"
		"\t\tlen -\tlength of addr range to verify\n"
	  );
