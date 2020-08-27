# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
#
#

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
ifneq ($(CONFIG_POWER),)
obj-y += pmic.o
endif
obj-$(CONFIG_IMX8M_DDR3L) += ddr3l_timing.o
else
obj-$(CONFIG_IMX8MM) += tx8mm.o
endif
