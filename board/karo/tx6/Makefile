#
# (C) Copyright 2009 DENX Software Engineering
# Author: John Rigby <jcrigby@gmail.com>
#
# SPDX-License-Identifier:	GPL-2.0+
#

include $(TOPDIR)/config.mk

LIB	= $(obj)lib$(BOARD).o

COBJS	:= tx6qdl.o
SOBJS	:= lowlevel_init.o
ifeq ($(CONFIG_CMD_ROMUPDATE),y)
	COBJS += flash.o
endif

SRCS	:= $(SOBJS:.o=.S) $(COBJS:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS))
SOBJS	:= $(addprefix $(obj),$(SOBJS))

$(LIB):	$(obj).depend $(OBJS) $(SOBJS)
	$(call cmd_link_o_target, $(OBJS) $(SOBJS))

#########################################################################

include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
