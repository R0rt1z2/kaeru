//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void led_set_brightness(unsigned int idx, unsigned int bg) {
    ((void (*)(unsigned int, unsigned int))(0x46022890 | 1))(idx, bg);
}

void mt_power_off(void) {
    ((void (*)(void))(0x46000170 | 1))();
}

void cmd_shutdown(const char* arg, void* data, unsigned sz) {
    fastboot_info("Shutting down...");
    fastboot_okay("");
    mt_power_off();
}

void rgb_led_set(uint8_t r, uint8_t g, uint8_t b) {
    led_set_brightness(0, r);  // RED
    led_set_brightness(1, g);  // GREEN
    led_set_brightness(2, b);  // BLUE
}

void patch_cmdline(void) {
    char* cmdline = (char*)0x46093BE8;
    char* cs = strstr(cmdline, "console=");

    if (cs) {
        char* eocs = strstr(cs + 8, "root=");

        if (eocs) {
            memmove(cs, eocs, strlen(eocs) + 1);
            printf("Patched kernel cmdline: %s\n", cmdline);
        }
    }
}

void board_early_init(void) {
    printf("Entering early init for Sony Xperia XA1 / Ultra / Plus\n");

    // Kept for backward compatibility with other LK builds, even though it's not
    // strictly necessary, other patches already target the functions used to check
    // whether the device is unlocked.
    //
    // This patch skips two checks in the erase and flash functions that would
    // otherwise block operations on a locked device.
    NOP(0x460299B0, 2);
    NOP(0x46029A08, 2);

    // In addition to seccfg, Sony uses its own method to store the device's lock state.
    // This state is stored in the Trim Area (TA) partition and read at runtime to
    // determine whether the device is locked or unlocked (s1_bl_unlocked).
    FORCE_RETURN(0x4602BC08, 1);

    // Probably not necessary either. This forces the SBC status, likely read from
    // an eFuse, to always return 0 (off), making the device appear as unfused.
    FORCE_RETURN(0x460601C8, 0);
}

void board_late_init(void) {
    // This patch makes the device report as secure but unlocked when running
    // 'fastboot getvar all'. I'm not entirely sure, but it seems that reporting
    // the device as not secure caused issues elsewhere in the bootloader.
    PATCH_MEM_ARM(0x4604a9c0,
                  0xe3b02000,  // movs r2, #0
                  0xe5c12000,  // strb r2, [r1, #0]
                  0xe3b00000,  // movs r0, #0
                  0xe12fff1e   // bx lr
    );

    if (mtk_detect_key(VOLUME_UP)) {
        // Not entirely sure why this is necessary, but without it the device
        // boots into service mode instead of fastboot. Volume Down doesn't work
        // here, as it always triggers recovery mode regardless of what you try
        // to force.
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        // Sony had the brilliant idea of setting up a thread that constantly checks
        // whether USB is connected while the device is in fastboot mode. If the
        // device is disconnected, it will forcefully reboot.
        //
        // This behavior is both unnecessary and annoying, so we patch the function
        // responsible to always return 0, preventing the reboot.
        FORCE_RETURN(0x46029A34, 0);

        // By default, when booting into fastboot, there is no display output.
        // This happens because the display driver is not initialized unless
        // the boot mode is something other than fastboot.
        //
        // In normal scenarios, the display driver gets initialized by a function
        // that is called to show certain logos on the screen.
        //
        // We take advantage of this by manually calling that function to show
        // a blank logo. This initializes the display driver and sets a black
        // background.
        ((void (*)(int, int))(0x4602931C | 1))(6, 1);

        // Sony uses a red LED for fastboot mode, which is not very user-friendly,
        // as it can be confused with low battery or other issues.
        //
        // This patch invalidates the original function that sets the LED color and
        // forces it to use a pink color instead. This is also useful for users to
        // identify that kaeru is up and running.
        NOP(0x46029D82, 2);
        NOP(0x46029D8A, 2);
        rgb_led_set(255, 0, 204);

        // There is no easy way to power off the device from fastboot mode.
        // Holding the power button simply reboots the device, forcing you to
        // boot into the OS to shut it down properly. This is not ideal, so
        // we add a fastboot command that allows powering off directly.
        //
        // This was shamelessly borrowed from Motorola’s (or Huaqin’s) LK image.
        // It simply calls mt_power_off().
        fastboot_register("oem shutdown", cmd_shutdown, 1);
        fastboot_register("oem poweroff", cmd_shutdown, 1);
    }

    if (get_bootmode() != BOOTMODE_NORMAL) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        //
        // Displaying the boot mode can be helpful for developers, as it provides
        // immediate feedback and can prevent debugging headaches.
        show_bootmode(get_bootmode());
    }

    if (get_bootmode() != BOOTMODE_FASTBOOT) {
        // Sets the boot status to green. This is likely ineffective, as it seems
        // to be overridden later by the bootloader. Still, it doesn't hurt to
        // include it here.
        WRITE32(0x460B4128, 0);

        // Sony does not use the standard bootloader warning messages to inform the
        // user that the device is unlocked, tampered, or otherwise modified.
        // Instead, they display custom logos with on-screen messages.
        //
        // This patch prevents LK from calling the function that shows those logos,
        // and skips directly to the next instruction. It disables both the red and
        // orange state warnings.
        NOP(0x4602990A, 2);
        NOP(0x46029912, 2);
        NOP(0x4602991A, 2);

#ifdef KAERU_DEBUG
        // This patch redirects several dprintf calls to use video_printf instead,
        // allowing messages to be printed directly to the screen rather than to
        // the serial console. This is useful for debugging when UART is not available.
        PATCH_CALL(0x4602C552, (void*)CONFIG_VIDEO_PRINTF_ADDRESS, TARGET_THUMB);
        PATCH_CALL(0x4602C4D0, (void*)CONFIG_VIDEO_PRINTF_ADDRESS, TARGET_THUMB);

        // Forcefully enable UART by patching out the function call that appends
        // "printk.disable_uart=1" to the kernel command line. While this could be
        // patched directly later, it's simpler to handle it here.
        NOP(0x4602D710, 2);

        // By default, the stock kernel expects the bootloader to provide all UART-related
        // parameters. This is rather shortsighted, as users are more likely to modify
        // the kernel than the bootloader.
        //
        // When testing custom kernels, which usually include their own UART settings, this
        // leads to conflicts, since the kernel gives priority to the first UART config
        // it sees. This patch ensures the kernel retains full control over UART settings,
        // allowing it to use its own configuration.
        patch_cmdline();
#endif
    }
}
