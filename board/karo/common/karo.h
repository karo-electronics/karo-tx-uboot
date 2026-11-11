/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2023 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <binman_sym.h>
#include <config.h>
#include <linux/types.h>

#if IS_ENABLED(CONFIG_XPL_BUILD)
binman_sym_extern(ulong, ddr_1d_imem_fw, size);
binman_sym_extern(ulong, ddr_1d_dmem_fw, size);
binman_sym_extern(ulong, ddr_2d_imem_fw, size);
binman_sym_extern(ulong, ddr_2d_dmem_fw, size);
#endif /* CONFIG_XPL_BUILD */

void karo_set_ethaddr(int index);
void karo_env_cleanup(void);

int karo_load_fdt(const char *fdt_file);
int karo_load_fdt_overlay(void *fdt, const char *dev_type, const char *dev_part,
			  const char *overlay);
#ifdef CONFIG_LED
void tx8m_led_init(void);
void tx93_led_init(void);
#else
static inline void tx8m_led_init(void)
{
}

static inline void tx93_led_init(void)
{
}
#endif

struct bd_info;
int ft_karo_common_setup(void *blob, struct bd_info *bd);

#if IS_ENABLED(CONFIG_PHYS_64BIT)
#define PAF_LN	"ll"
#else
#define PAF_LN	"l"
#endif
#define PAF "%" PAF_LN "x"
#define PAFU "%" PAF_LN "u"

/* Control DT selection from the stored 'baseboard' (KARO_DTB_SELECT) */
#if IS_ENABLED(CONFIG_KARO_DTB_SELECT)

/* where the 'baseboard' value came from */
enum karo_dtb_source {
	KARO_DTB_SRC_NONE = 0,	/* not run yet */
	KARO_DTB_SRC_STORED,	/* stored environment, CRC valid */
	KARO_DTB_SRC_DEFAULT,	/* CONFIG_KARO_DEFAULT_BASEBOARD */
	KARO_DTB_SRC_MODULE,	/* nothing usable: bare module DT */
};

/* selection result, kept in .data */
struct karo_dtb_state {
	enum karo_dtb_source src;
	int read_err;		/* errno of the media read, 0 if it worked */
	char baseboard[32];
	char dt_name[64];	/* FIT config description to match */
};

/*
 * Read env 'baseboard' from the medium selected at build time by
 * CONFIG_ENV_IS_IN_*: 0, -ENOENT, -EBADMSG, media error
 */
int karo_env_baseboard(char *out, size_t len);

/* run the selection once; idempotent; safe before relocation */
const struct karo_dtb_state *karo_dtb_select(void);

/* board_late_init(): print the running control DT and its origin */
void karo_dtb_report(void);
#else
static inline void karo_dtb_report(void)
{
}
#endif
