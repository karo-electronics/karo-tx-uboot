#
# (C) Copyright 2015 Lothar Waßmann <LW@KARO-electronics.de>
#
# SPDX-License-Identifier:	GPL-2.0+
#

LDSCRIPT := $(BOARDDIR)/u-boot.lds

obj-y				+= lowlevel_init.o pmic.o tx6qdl.o
obj-$(CONFIG_LTC3676)		+= ltc3676.o
obj-$(CONFIG_RN5T567)		+= rn5t567.o
obj-$(CONFIG_RN5T618)		+= rn5t618.o

obj-$(CONFIG_CMD_ROMUPDATE) 	+= flash.o
