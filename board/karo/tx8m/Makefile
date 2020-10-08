# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
#
#

ifdef CONFIG_SPL_BUILD
obj-y += spl.o

obj-$(CONFIG_POWER_PCA9450) += pmic_pca9450.o
obj-$(CONFIG_POWER_BD71837) += pmic_bd71847.o
obj-$(CONFIG_KARO_QS8M)     += pmic_rn5t567.o

obj-$(CONFIG_IMX8M_DDR3L)    += ddr3l_timing.o
obj-$(CONFIG_KARO_QSXM_MM60) += lpddr4_timing_qsxm.o
obj-$(CONFIG_KARO_QSXP_ML81) += lpddr4_timing_qsxp.o
else
obj-$(CONFIG_IMX8MM) += tx8mm.o
obj-$(CONFIG_IMX8MN) += tx8mn.o
obj-$(CONFIG_IMX8MP) += tx8mp.o
endif
