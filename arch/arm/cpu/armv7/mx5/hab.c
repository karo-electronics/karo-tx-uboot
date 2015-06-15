/*
 * Copyright (C) 2010-2014 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hab.h>

HAB_FUNC(entry, hab_status_t)
HAB_FUNC(exit, hab_status_t)
HAB_FUNC5(authenticate_image, void *, uint8_t, size_t, void **, size_t *, hab_loader_callback_f_t)
HAB_FUNC2(report_status, hab_status_t, hab_config_t *, hab_state_t *)
HAB_FUNC4(report_event, hab_status_t, hab_status_t, uint32_t, uint8_t *, size_t *)
HAB_FUNC3(check_target, hab_status_t, uint8_t, const void *, size_t)
HAB_FUNC3(assert, hab_status_t, uint8_t, const void *, size_t)

struct mx53_ivt {
	u32 header;
	u32 app_start_addr;
	u32 rsrvd1;
	void *dcd_ptr;
	struct mx53_boot_data *boot_data;
	void *self;
	void *csf;
	u32 rsrvd2;
};

struct mx53_boot_data {
	void *start;
	u32 length;
	u32 plugin;
};

#define IVT_SIZE		0x20
#define ALIGN_SIZE		0x400
#define CSF_PAD_SIZE		0x2000

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

static inline int read_fuse(unsigned bank, unsigned row)
{
	struct iim_regs *iim_regs = (void *)IMX_IIM_BASE;
	u32 *fuses;

	if (bank > ARRAY_SIZE(iim_regs->bank))
		return -EINVAL;
	fuses = iim_regs->bank[bank].fuse_regs;

	debug("Reading fuse bank %u row %u @ %p\n", bank, row, &fuses[row]);
	return readl(&fuses[row]);
}

static bool is_hab_enabled(void)
{
#ifdef DEBUG
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
#endif
	return read_fuse(0, 4) & (1 << 1);
}

static void mx53_hab_display_event(uint8_t *event_data, size_t bytes)
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
}

static inline void mx53_hab_pr_fuse_val(unsigned bank, unsigned row)
{
	printf(" %02x", read_fuse(bank, row));
}

static inline void mx53_hab_dump_srk_hash(void)
{
	int i;

	printf("SRK hash:\n\t");
	mx53_hab_pr_fuse_val(1, 1);
	for (i = 1; i <= 31; i++) {
		if (i % 8 == 0)
			printf("\n\t");
		mx53_hab_pr_fuse_val(3, i);
	}
	printf("\n");
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
	int hab_debug = getenv_yesno("hab_debug") == 1;

	if (hab_debug) {
		if (is_hab_enabled()) {
			printf("Secure boot enabled\n\t");
		} else {
			printf("Secure boot disabled\n");
		}
		mx53_hab_dump_srk_hash();
	}

	/* Check HAB status */
	config = state = 0; /* ROM code assumes short enums! */
	ret = hab_rvt_report_status(&config, &state);
	if (hab_debug || ret != HAB_SUCCESS)
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
			mx53_hab_display_event(event_data, bytes);
			puts("\n");
			bytes = sizeof(event_data);
			index++;
		}
		ret = index - last_hab_event;
		last_hab_event = index;
	} else {
		/* Display message if no HAB events are found */
		if (hab_debug)
			puts("No HAB Events Found!\n");
		ret = 0;
	}
	return ret;
}

static inline hab_status_t hab_init(void)
{
	hab_status_t ret;

	ret = hab_rvt_entry();
	debug("hab_rvt_entry() returned %02x\n", ret);
	if (ret != HAB_SUCCESS) {
		printf("hab entry function failed: %02x\n", ret);
	}

	return ret;
}

static inline hab_status_t hab_exit(void)
{
	hab_status_t ret;

	ret = hab_rvt_exit();
	if (ret != HAB_SUCCESS)
		printf("hab exit function failed: %02x\n", ret);

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
	hab_target_t type = HAB_TGT_MEMORY;
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
