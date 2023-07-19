# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2023 Lothar Waßmann <LW@KARO-electronics.de>
#
#

obj-y += tx93.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_KARO_TX93_5210) += lpddr4_timing_tx93.o
obj-$(CONFIG_KARO_QS93_5210) += lpddr4_timing_qs93.o
endif
