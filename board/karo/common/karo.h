/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2023 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <config.h>

#if IS_ENABLED(CONFIG_XPL_BUILD)
#if CONFIG_SPL_SIZE_LIMIT > 0 &&					\
	CONFIG_VAL(SIZE_LIMIT) > (SPL_OCRAM_SIZE - SPL_DTB_SIZE - SPL_DDRFW_SIZE)
#error CONFIG_SPL_SIZE_LIMIT exceeds available OCRAM space
#endif /* CONFIG_SPL_SIZE_LIMIT > 0 ... */
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
