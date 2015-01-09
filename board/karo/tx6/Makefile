#
# (C) Copyright 2009 DENX Software Engineering
# Author: John Rigby <jcrigby@gmail.com>
#
# SPDX-License-Identifier:	GPL-2.0+
#

include $(TOPDIR)/config.mk

LIB	= $(obj)lib$(BOARD).o

ifeq ($(CONFIG_TX6_V2),)
COBJS-y	:= tx6qdl.o
else
COBJS-y	:= tx6-1020.o
endif
COBJS-$(CONFIG_LTC3676)	+= ltc3676.o
COBJS-$(CONFIG_RN5T618)	+= rn5t618.o

COBJS-$(CONFIG_CMD_ROMUPDATE) += flash.o

SOBJS-y	:= lowlevel_init.o

SRCS	:= $(SOBJS:.o=.S) $(COBJS-y:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS-y))
SOBJS	:= $(addprefix $(obj),$(SOBJS-y))

$(LIB):	$(obj).depend $(OBJS) $(SOBJS)
	$(call cmd_link_o_target, $(OBJS) $(SOBJS))

#########################################################################

include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
