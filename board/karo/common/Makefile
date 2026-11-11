# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
#
#

ifndef CONFIG_XPL_BUILD
obj-y += env.o fdt.o
obj-$(CONFIG_LED) += led.o
obj-$(CONFIG_KARO_DTB_SELECT) += dtb_select.o
else
obj- := __dummy__.o
obj-$(CONFIG_SPL_KARO_DTB_SELECT) += dtb_select.o
endif
