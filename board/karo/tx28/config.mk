# stack is allocated below CONFIG_SYS_TEXT_BASE
CONFIG_SYS_TEXT_BASE := 0x40001000
CONFIG_SPL_TEXT_BASE := 0x00000000

PLATFORM_CPPFLAGS += -Werror
ifneq ($(CONFIG_SPL_BUILD),y)
	ALL-y += $(obj)u-boot.sb
endif
