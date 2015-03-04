#
# (C) Copyright 2012-2015 Lothar Waßmann <LW@KARO-electronics.de>
#
# SPDX-License-Identifier:	GPL-2.0+
#

ifeq ($(CONFIG_SPL_BUILD),)
	obj-$(CONFIG_OF_LIBFDT)		+= fdt.o
	obj-$(CONFIG_SPLASH_SCREEN)	+= splashimage.o
endif
obj-$(CONFIG_CMD_NAND)			+= nand.o
obj-$(CONFIG_ENV_IS_IN_MMC)		+= mmc.o
obj-y 					+= env.o
