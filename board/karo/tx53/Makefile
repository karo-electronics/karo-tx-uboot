#
# (C) Copyright 2015 Lothar Waßmann <LW@KARO-electronics.de>
#
# SPDX-License-Identifier:	GPL-2.0+
#

LDSCRIPT := $(BOARDDIR)/u-boot.lds

obj-y				+= lowlevel_init.o tx53.o
obj-$(CONFIG_CMD_ROMUPDATE)	+= flash.o
