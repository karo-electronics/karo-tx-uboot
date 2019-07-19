/*
 * Copyright (C) 2010-2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <common.h>
#include <fuse.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hab.h>

HAB_FUNC(entry, enum hab_status)
HAB_FUNC(exit, enum hab_status)
HAB_FUNC5(authenticate_image, void *, uint8_t, size_t, void **, size_t *, hab_loader_callback_f_t)
//HAB_FUNC1(run_dcd, enum hab_status, const uint8_t *)
HAB_FUNC2(run_csf, enum hab_status, const uint8_t *, uint8_t)
HAB_FUNC2(report_status, enum hab_status, enum hab_config *, enum hab_state *)
HAB_FUNC4(report_event, enum hab_status, enum hab_status, uint32_t, uint8_t *, size_t *)
HAB_FUNC3(check_target, enum hab_status, uint8_t, const void *, size_t)
HAB_FUNC3(assert, enum hab_status, uint8_t, const void *, size_t)

#define MAX_RECORD_BYTES     (8 * 1024)

#define MX6DQ_PU_IROM_MMU_EN_VAR	0x009024a8
#define MX6SDL_PU_IROM_MMU_EN_VAR	0x00901dd0
#define MX6SL_PU_IROM_MMU_EN_VAR	0x00900a18

struct record {
	uint8_t  tag;				/* Tag */
	uint8_t  len[2];			/* Length */
	uint8_t  par;				/* Version */
	uint8_t  contents[MAX_RECORD_BYTES];	/* Record Data */
	bool	 any_rec_flag;
};

char *rsn_str[] = {
	"RSN = HAB_RSN_ANY (0x00)\n",
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
};

char *sts_str[] = {
	"STS = HAB_STS_ANY (0x00)\n",
	"STS = HAB_FAILURE (0x33)\n",
	"STS = HAB_WARNING (0x69)\n",
	"STS = HAB_SUCCESS (0xF0)\n",
	"STS = INVALID\n",
};

char *eng_str[] = {
	"ENG = HAB_ENG_ANY (0x00)\n",
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
};

char *ctx_str[] = {
	"CTX = HAB_CTX_ANY(0x00)\n",
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
};

uint8_t hab_statuses[ARRAY_SIZE(sts_str)] = {
	HAB_STS_ANY,
	HAB_FAILURE,
	HAB_WARNING,
	HAB_SUCCESS,
	-1
};

uint8_t hab_reasons[ARRAY_SIZE(rsn_str)] = {
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

uint8_t hab_contexts[ARRAY_SIZE(ctx_str)] = {
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

uint8_t hab_engines[ARRAY_SIZE(eng_str)] = {
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
	uint32_t reg;
	static int first = 1;

	if (fuse_read(0, 6, &reg)) {
		printf("Failed to read SECURE_BOOT fuse\n");
		return 0;
	}

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

static uint32_t last_hab_event __attribute__((section(".data")));

int get_hab_status(void)
{
	uint32_t index = last_hab_event; /* Loop index */
	uint8_t event_data[128]; /* Event data buffer */
	size_t bytes = sizeof(event_data); /* Event size in bytes */
	enum hab_config config = 0;
	enum hab_state state = 0;
	static int first = 1;
	int ret;

	if (first) {
		printf("Secure boot %sabled\n",
		       is_hab_enabled() ? "en" : "dis");
	}

	/* Check HAB status */
	enable_ocotp_clk(1);
	ret = hab_rvt_report_status(&config, &state);
	enable_ocotp_clk(0);
	if (first)
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
	first = 0;
	return ret;
}

static inline void hab_clear_events(void)
{
	uint32_t index = last_hab_event; /* Loop index */
	uint8_t event_data[128]; /* Event data buffer */
	size_t bytes = sizeof(event_data); /* Event size in bytes */
	enum hab_config config = 0;
	enum hab_state state = 0;
	int ret;

	/* Check HAB status */
	enable_ocotp_clk(1);
	ret = hab_rvt_report_status(&config, &state);
	enable_ocotp_clk(0);
	if (ret != HAB_SUCCESS) {
		while (hab_rvt_report_event(HAB_STS_ANY, index, event_data,
					    &bytes) == HAB_SUCCESS) {
			index++;
		}
		last_hab_event = index;
	}
}

static inline enum hab_status hab_init(void)
{
	enum hab_status ret;

	enable_ocotp_clk(1);

	hab_caam_clock_enable(1);

	ret = hab_rvt_entry();
	debug("hab_rvt_entry() returned %02x\n", ret);
	if (ret != HAB_SUCCESS) {
		printf("hab entry function failed: %02x\n", ret);
		hab_caam_clock_enable(0);
	}

	return ret;
}

static inline enum hab_status hab_exit(void)
{
	enum hab_status ret;

	ret = hab_rvt_exit();
	if (ret != HAB_SUCCESS)
		printf("hab exit function failed: %02x\n", ret);

	hab_caam_clock_enable(0);
	enable_ocotp_clk(0);

	return ret;
}

static enum hab_status hab_check_target(enum hab_target type, uint32_t addr, size_t len)
{
	enum hab_status ret;

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

static enum hab_status hab_assert(uint32_t type, uint32_t addr, size_t len)
{
	enum hab_status ret;

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
	enum hab_target type = HAB_TGT_ANY;
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

/* Get CSF Header length */
static int get_hab_hdr_len(struct hab_hdr *hdr)
{
	return (size_t)((hdr->len[0] << 8) + (hdr->len[1]));
}

/* Check whether addr lies between start and
 * end and is within the length of the image
 */
static int chk_bounds(u8 *addr, size_t bytes, u8 *start, u8 *end)
{
	size_t csf_size = (size_t)((end + 1) - addr);

	return addr && (addr >= start) && (addr <= end) &&
		(csf_size >= bytes);
}

/* Get Length of each command in CSF */
static int get_csf_cmd_hdr_len(u8 *csf_hdr)
{
	if (*csf_hdr == HAB_CMD_HDR)
		return sizeof(struct hab_hdr);

	return get_hab_hdr_len((struct hab_hdr *)csf_hdr);
}

/* Check if CSF is valid */
static inline bool csf_is_valid(struct ivt *ivt, void *addr, size_t bytes)
{
	u8 *start = addr;
	u8 *csf_hdr;
	u8 *end;

	size_t csf_hdr_len;
	size_t cmd_hdr_len;
	size_t offset = 0;

	if (bytes != 0)
		end = start + bytes - 1;
	else
		end = start;

	/* Verify if CSF pointer content is zero */
	if (!ivt->csf) {
		puts("Error: CSF pointer is NULL\n");
		return false;
	}

	csf_hdr = (u8 *)(uintptr_t)ivt->csf;

	/* Verify if CSF Header exist */
	if (*csf_hdr != HAB_CMD_HDR) {
		puts("Error: CSF header command not found\n");
		return false;
	}

	csf_hdr_len = get_hab_hdr_len((struct hab_hdr *)csf_hdr);

	/* Check if the CSF lies within the image bounds */
	if (!chk_bounds(csf_hdr, csf_hdr_len, start, end)) {
		puts("Error: CSF lies outside the image bounds\n");
		return false;
	}

	do {
		struct hab_hdr *cmd;

		cmd = (struct hab_hdr *)&csf_hdr[offset];

		switch (cmd->tag) {
		case (HAB_CMD_WRT_DAT):
			puts("Error: Deprecated write command found\n");
			return false;
		case (HAB_CMD_CHK_DAT):
			puts("Error: Deprecated check command found\n");
			return false;
		case (HAB_CMD_SET):
			if (cmd->par == HAB_PAR_MID) {
				puts("Error: Deprecated Set MID command found\n");
				return false;
			}
		default:
			break;
		}

		cmd_hdr_len = get_csf_cmd_hdr_len(&csf_hdr[offset]);
		if (!cmd_hdr_len) {
			puts("Error: Invalid command length\n");
			return false;
		}
		offset += cmd_hdr_len;

	} while (offset < csf_hdr_len);

	return true;
}

static int ivt_header_error(const char *err_str, struct ivt_header *ivt_hdr)
{
	printf("%s magic=0x%x length=0x%02x version=0x%x\n", err_str,
	       ivt_hdr->magic, ivt_hdr->length, ivt_hdr->version);

	return 1;
}

static int verify_ivt_header(struct ivt_header *ivt_hdr)
{
	if (ivt_hdr->magic != IVT_HEADER_MAGIC)
		return ivt_header_error("bad IVT magic", ivt_hdr);

	if (be16_to_cpu(ivt_hdr->length) != IVT_TOTAL_LENGTH)
		return ivt_header_error("bad IVT length", ivt_hdr);

	if (ivt_hdr->version != IVT_HEADER_V1 &&
	    ivt_hdr->version != IVT_HEADER_V2)
		return ivt_header_error("bad IVT version", ivt_hdr);

	return 0;
}

/*
 * Validate IVT structure of the image being authenticated
 */
static int validate_ivt(struct ivt *ivt)
{
	struct ivt_header *ivt_hdr = &ivt->hdr;

	if ((uintptr_t)ivt & 0x3) {
		puts("Error: IVT address is not 4 byte aligned\n");
		return 0;
	}

	/* Check IVT fields before allowing authentication */
	if ((!verify_ivt_header(ivt_hdr)) &&
	    (ivt->entry != 0x0) &&
	    (ivt->reserved1 == 0x0) &&
	    (ivt->self == (uint32_t)ivt) &&
	    (ivt->csf != 0x0) &&
	    (ivt->reserved2 == 0x0) &&
	    (ivt->dcd == 0x0)) {
		return 1;
	}

	puts("Error: Invalid IVT structure\n");
	if (ivt->entry == 0x0)
		printf("entry address must not be NULL\n");
	if (ivt->reserved1 != 0x0)
		printf("reserved word at offset 0x%02x should be NULL\n",
		       offsetof(struct ivt, reserved1));
	if (ivt->dcd != 0x0)
		puts("Error: DCD pointer must be 0\n");
	if (ivt->self != (uint32_t)ivt)
		printf("SELF pointer does not point to IVT\n");
	if (ivt->csf == 0x0)
		printf("CSF address must not be NULL\n");
	if (ivt->reserved2 != 0x0)
		printf("reserved word at offset 0x%02x should be NULL\n",
		       offsetof(struct ivt, reserved2));
	puts("\nAllowed IVT structure:\n");
	printf("IVT HDR       = 0x4X2000D1 (0x%08x)\n", *((u32 *)&ivt->hdr));
	printf("IVT ENTRY     = 0xXXXXXXXX (0x%08x)\n", ivt->entry);
	printf("IVT RSV1      = 0x00000000 (0x%08x)\n", ivt->reserved1);
	printf("IVT DCD       = 0x00000000 (0x%08x)\n", ivt->dcd);
	printf("IVT BOOT_DATA = 0xXXXXXXXX (0x%08x)\n", ivt->boot);
	printf("IVT SELF      = 0xXXXXXXXX (0x%08x)\n", ivt->self);
	printf("IVT CSF       = 0xXXXXXXXX (0x%08x)\n", ivt->csf);
	printf("IVT RSV2      = 0x00000000 (0x%08x)\n", ivt->reserved2);

	/* Invalid IVT structure */
	return 0;
}

bool imx_hab_authenticate_image(void *addr, size_t len, size_t ivt_offset)
{
	struct ivt *ivt;
	enum hab_status status;

	if (len <= sizeof(ivt)) {
		printf("Invalid image size %08x\n", len);
		return false;
	}
	if (ivt_offset >= len - sizeof(*ivt)) {
		printf("IVT offset must be smaller than image size - %u\n",
		       sizeof(*ivt));
		return false;
	}

	ivt = addr + ivt_offset;
	/* Verify IVT header bugging out on error */
	if (!validate_ivt(ivt)) {
		print_buffer((ulong)ivt, ivt, 4, 0x8, 0);
		return false;
	}

	if (!csf_is_valid(ivt, addr, len)) {
		print_buffer(ivt->csf, (void *)(ivt->csf), 4, 0x10, 0);
		return false;
	}

	status = hab_init();
	if (status != HAB_SUCCESS)
		goto hab_auth_error;

	hab_clear_events();

	status = hab_rvt_check_target(HAB_TGT_MEMORY, addr, len);
	if (status != HAB_SUCCESS) {
		printf("HAB check target 0x%p-0x%p failed\n",
		       addr, addr + len);
		goto hab_auth_error;
	}
	get_hab_status();

	/*
	 * If the MMU is enabled, we have to notify the ROM
	 * code, or it won't flush the caches when needed.
	 * This is done, by setting the "pu_irom_mmu_enabled"
	 * word to 1. You can find its address by looking in
	 * the ROM map. This is critical for
	 * authenticate_image(). If MMU is enabled, without
	 * setting this bit, authentication will fail and may
	 * crash.
	 */
	/* Check MMU enabled */
	if (get_cr() & CR_M) {
		if (is_cpu_type(MXC_CPU_MX6Q) || is_cpu_type(MXC_CPU_MX6D)) {
			/*
			 * This won't work on Rev 1.0.0 of
			 * i.MX6Q/D, since their ROM doesn't
			 * do cache flushes. don't think any
			 * exist, so we ignore them.
			 */
			if (!is_mx6dqp())
				writel(1, MX6DQ_PU_IROM_MMU_EN_VAR);
		} else if (is_cpu_type(MXC_CPU_MX6SOLO) ||
			   is_cpu_type(MXC_CPU_MX6DL)) {
			writel(1, MX6SDL_PU_IROM_MMU_EN_VAR);
		} else if (is_cpu_type(MXC_CPU_MX6SL)) {
			writel(1, MX6SL_PU_IROM_MMU_EN_VAR);
		}
	}

	hab_rvt_authenticate_image(HAB_CID_UBOOT, ivt_offset, &addr,
				   &len, NULL);
	status = hab_exit();
	if (status != HAB_SUCCESS)
		goto hab_auth_error;

	if (get_hab_status() != 0) {
		printf("ERROR: hab_authenticate_image failed\n");
		return false;
	}
	printf("HAB authentication of image at %p..%p successful\n",
	       addr, addr + len);
	return true;

 hab_auth_error:
	printf("ERROR: HAB authentication failure: %02x\n", status);
	get_hab_status();
	return false;
}

static int do_authenticate_image(cmd_tbl_t *cmdtp, int flag, int argc,
				 char *const argv[])
{
	void *addr;
	size_t ivt_offset, len;

	if (argc < 4)
		return CMD_RET_USAGE;

	addr = (void *)simple_strtoul(argv[1], NULL, 16);
	len = simple_strtoul(argv[2], NULL, 16);
	ivt_offset = simple_strtoul(argv[3], NULL, 16);

	if (imx_hab_authenticate_image(addr, len, ivt_offset))
		return CMD_RET_SUCCESS;

	get_hab_status();

	return CMD_RET_FAILURE;
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

U_BOOT_CMD(
	   hab_auth_img, 4, 0, do_authenticate_image,
	   "authenticate image via HAB",
	   "addr len ivt_offset\n"
	   "addr - image hex address\n"
	   "len - image size (hex)\n"
	   "ivt_offset - hex offset of IVT in the image"
	   );
