//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define S1BOOT_DATA_ADDRESS 0x46093BD8

typedef enum {
    BOOT_NORMAL_IMAGE = 0,
    BOOT_DUMPER_IMAGE = 1,
    BOOT_FOTA_IMAGE = 2,
    BOOT_S1_SERVICE_IMAGE = 3,
    BOOT_FASTBOOT_IMAGE = 4,
    BOOT_RECOVERY_IMAGE = 5
} s1boot_image_t;

typedef struct s1boot_data {
    s1boot_image_t image_to_boot;    // controls which image to boot (BOOT_NORMAL_IMAGE, etc.)
    char *s1_cmdline_additions;      // additional cmdline parameters to append
    uint32_t boot_aid;               // some kind of boot authentication/authorization ID
    uint32_t override_mach;          // ARM machine type override (0 = don't override)
} s1boot_data_t;

char* get_image_to_boot(int image_id) {
    switch (image_id) {
        case 0:
            return "NORMAL";
        case 1:
            return "DUMPER";
        case 2:
            return "FOTA";
        case 3:
            return "S1 SERVICE";
        case 4:
            return "FASTBOOT";
        case 5:
            return "RECOVERY";
        default:
            return "UNKNOWN";
    }
}

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

int mt_get_gpio_in(uint32_t pin) {
    return ((int (*)(uint32_t))(0x460006C0 | 1))(pin);
}

int is_volume_down_pressed(void) {
    return mt_get_gpio_in(0x80000000 + 104) == 0; // GPIO_ACTIVE_LOW
}

s1boot_data_t* get_s1boot_data(void) {
    return (s1boot_data_t*)S1BOOT_DATA_ADDRESS;
}

s1boot_image_t s1boot_get_bootimage(void) {
    return get_s1boot_data()->image_to_boot;
}

void s1boot_set_bootimage(s1boot_image_t mode) {
    get_s1boot_data()->image_to_boot = mode;
}

void s1boot_bootimage_shim(void) {
    s1boot_data_t* s1boot_data = get_s1boot_data();

    printf("Image to boot: %s (%d)\n",
           get_image_to_boot(s1boot_data->image_to_boot),
           s1boot_data->image_to_boot);
    printf("Cmdline additions: %s\n", s1boot_data->s1_cmdline_additions ?: "None");
    printf("Boot AID: 0x%08X\n", s1boot_data->boot_aid);
    printf("Override machine type: %s\n",
           s1boot_data->override_mach ? "enabled" : "disabled");

    if (mtk_detect_key(VOLUME_UP)) {
        printf("Volume Up detected, forcing recovery mode\n");
        s1boot_set_bootimage(BOOT_RECOVERY_IMAGE);
    } else if (is_volume_down_pressed()) {
        printf("Volume Down detected, forcing fastboot mode\n");
        s1boot_set_bootimage(BOOT_FASTBOOT_IMAGE);
    }
}

void board_early_init(void) {
    printf("Entering early init for Sony Xperia XA1 / Ultra / Plus\n");

    // Sony has multiple boot mode handlers, and they are extremely broken and
    // annoying to deal with.
    //
    // Because of this, we inject a hook right in the middle of the main boot mode
    // detection function so we can do our own handling of volume keys and boot modes.
    PATCH_CALL(0x460296D8, (void*)s1boot_bootimage_shim, TARGET_THUMB);

    // There is some annoying USB check inside of platform_init() that reboots the
    // device if the charger / USB has been disconnected between Preloader and LK
    // execution.
    //
    // This is pretty overkill when booting Preloaders with plstage, since we quickly
    // disconnect USB after sending the Preloader.
    NOP(0x4600234C, 2);

    // Sony controls UART output via the Trim Area. This is controlled by the 0x9A9
    // TA unit. When this unit is read, UART output gets disabled if the value
    // says so.
    //
    // This patch forces dprintf calls to skip the check completely, so UART
    // stays enabled no matter what's in the TA.
    NOP(0x46002978, 1);

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

    if (s1boot_get_bootimage() == BOOT_FASTBOOT_IMAGE) {
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

        // Show the current boot mode on the screen for clarity.
        show_bootmode(BOOTMODE_FASTBOOT);
    }

    if (s1boot_get_bootimage() != BOOT_FASTBOOT_IMAGE) {
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