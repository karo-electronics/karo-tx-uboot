#
# (C) Copyright 2015 Lothar Waßmann <LW@KARO-electronics.de>
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y				+= tx28.o
obj-$(CONFIG_SPL_BUILD)		+= spl_boot.o
ifneq ($(CONFIG_SPL_BUILD),y)
	obj-$(CONFIG_CMD_ROMUPDATE) += flash.o
endif
