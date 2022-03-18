/* SPDX-License-Identifier: GPL-2.0
 *
 * (C) Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

struct node_info;

void env_cleanup(void);
void karo_fdt_move_fdt(void);
void karo_fixup_mtdparts(void *blob, struct node_info *info, size_t count);
