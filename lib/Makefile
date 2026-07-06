lib-y += debug.o
lib-y += common.o bootargs.o bootmode.o fastboot.o recovery.o
lib-y += libc/string.o

lib-$(CONFIG_THREAD_SUPPORT) += thread.o
lib-$(CONFIG_ENVIRONMENT_SUPPORT) += environment.o
lib-$(CONFIG_SPOOF_SUPPORT) += spoof.o

lib-$(CONFIG_FRAMEBUFFER_SUPPORT) += framebuffer/framebuffer.o
lib-$(CONFIG_FRAMEBUFFER_SUPPORT) += framebuffer/fonts/font_8x8_basic.o
lib-$(CONFIG_FONT_8X8_CALSTONE) += framebuffer/fonts/font_8x8_calstone.o
lib-$(CONFIG_FONT_8X8_COMIC_FANS) += framebuffer/fonts/font_8x8_comic_fans.o

lib-$(CONFIG_SEJ_SUPPORT) += crypto/xor.o crypto/sha256.o
lib-$(CONFIG_SEJ_SUPPORT) += security/sej/sej.o security/sej/sej_hk.o security/sej/sej_sk.o
lib-$(CONFIG_SEJ_SUPPORT) += security/seccfg.o
