obj-$(CONFIG_SYS_MTDPARTS_RUNTIME) += board_mtdparts.o
obj-$(CONFIG_DFU) += dfu.o
obj-y += env.o
ifeq ($(CONFIG_SPL_BUILD),)
	obj-y += fdt.o
endif
