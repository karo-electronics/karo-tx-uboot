# stack is allocated below CONFIG_SYS_TEXT_BASE
CONFIG_SYS_TEXT_BASE := 0x700ff000

__HAVE_ARCH_GENERIC_BOARD := y
PLATFORM_CPPFLAGS += -Werror
LOGO_BMP = logos/karo.bmp
OBJCOPYFLAGS += -j .pad
