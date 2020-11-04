# SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
#
# Copyright (C) 2018, STMicroelectronics - All Rights Reserved
#

ifeq ($(CONFIG_SPL_BUILD),)
	obj-y += txmp.o
endif
obj-$(CONFIG_DEBUG_UART_BOARD_INIT) += dbg-uart.o
obj-$(CONFIG_SYS_MTDPARTS_RUNTIME) += board_mtdparts.o
obj-$(CONFIG_DFU) += dfu.o
