# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
#
#

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
ifneq ($(CONFIG_SPL_DM_PMIC),)
	obj-$(CONFIG_SPL_DM_PMIC_PCA9450) += pmic_pca9450.o
	obj-$(CONFIG_SPL_DM_PMIC_BD71837) += pmic_bd71847.o
	obj-$(CONFIG_PMIC_RN5T567)     += pmic_rn5t567.o
endif
obj-$(CONFIG_IMX8M_DDR3L)    += ddr3l_timing.o
obj-$(CONFIG_KARO_QSXM_MM60) += lpddr4_timing_qsxm.o
obj-$(CONFIG_KARO_QSXP_ML81) += lpddr4_timing_qsxp.o
obj-$(CONFIG_KARO_TX8P_ML81) += lpddr4_timing_tx8p-ml81.o
obj-$(CONFIG_KARO_TX8P_ML82) += lpddr4_timing_tx8p-ml82.o
else
obj-$(CONFIG_IMX8MM) += tx8mm.o
obj-$(CONFIG_IMX8MN) += tx8mn.o
obj-$(CONFIG_IMX8MP) += tx8mp.o
endif
