/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2023 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

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
