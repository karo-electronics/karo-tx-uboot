#
# (C) Copyright 2015 Lothar Waßmann <LW@KARO-electronics.de>
#
# SPDX-License-Identifier:	GPL-2.0+
#

LDSCRIPT := $(BOARDDIR)/u-boot.lds

obj-y				+= pmic.o
ifeq ($(CONFIG_SOC_MX6UL),y)
	obj-y += tx6ul_ll_init.o tx6ul.o
else
	obj-y += lowlevel_init.o tx6qdl.o
	obj-$(CONFIG_LTC3676)	+= ltc3676.o
endif
obj-$(CONFIG_RN5T567)		+= rn5t567.o
obj-$(CONFIG_RN5T618)		+= rn5t618.o

obj-$(CONFIG_CMD_ROMUPDATE) 	+= flash.o
