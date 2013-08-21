# stack is allocated below CONFIG_SYS_TEXT_BASE
CONFIG_SYS_TEXT_BASE := 0x40100000
CONFIG_SPL_TEXT_BASE := 0x00000000

LOGO_BMP = logos/karo.bmp

PLATFORM_CPPFLAGS += -Werror
ifneq ($(CONFIG_SPL_BUILD),y)
	ALL-y += $(obj)u-boot.sb
endif
