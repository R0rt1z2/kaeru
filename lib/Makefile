lib-y += libc/string.o

lib-y += debug/debug.o

lib-y += common.o bootmode.o fastboot.o
lib-$(CONFIG_THREAD_SUPPORT) += thread.o
lib-$(CONFIG_ENVIRONMENT_SUPPORT) += environment.o

lib-$(CONFIG_SPOOF_SUPPORT) += spoof/spoof.o

lib-$(CONFIG_FRAMEBUFFER_SUPPORT) += framebuffer/framebuffer.o
lib-$(CONFIG_FRAMEBUFFER_SUPPORT) += framebuffer/fonts/font_8x8_basic.o
lib-$(CONFIG_FONT_8X8_CALSTONE) += framebuffer/fonts/font_8x8_calstone.o
lib-$(CONFIG_FONT_8X8_COMIC_FANS) += framebuffer/fonts/font_8x8_comic_fans.o

lib-$(CONFIG_LIBSEJ_SUPPORT) += libsej/sej.o