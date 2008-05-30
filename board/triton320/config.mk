#PLATFORM_CPPFLAGS+= -DDEBUG
PLATFORM_CPPFLAGS += -march=armv5 -mtune=xscale
PLATFORM_CPPFLAGS += -mno-thumb-interwork
PLATFORM_CPPFLAGS += -mabi=apcs-gnu -mno-thumb-interwork
PLATFORM_RELFLAGS += -msoft-float -Wa,-mfpu=vfp

TEXT_BASE = 0xa3fc0000
