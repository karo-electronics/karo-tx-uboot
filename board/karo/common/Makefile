# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
#
#

ifndef CONFIG_SPL_BUILD
obj-y += env.o fdt.o
obj-$(CONFIG_LED) += led.o
else
obj- := __dummy__.o
endif
