# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2024 Ka-Ro electronics GmbH
#

ifdef CONFIG_XPL_BUILD
	obj-y += spl.o
	obj-$(CONFIG_KARO_QS91_3110) += lpddr4_timing_qs91.o
	obj-$(CONFIG_KARO_TX91_3110) += lpddr4_timing_tx91.o
else
	obj-y += tx91.o
endif
