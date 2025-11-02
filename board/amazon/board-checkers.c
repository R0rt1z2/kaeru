//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define KEY_PRIVACY 0x2F

int video_get_rows(void) {
    return ((int (*)())(0x4BD2C340 | 1))();
}

void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x4BD2C308 | 1))(row, col);
}

struct device_t* mt_part_get_device(void) {
    return ((struct device_t* (*)(void))(0x4BD1FDE4|1))();
}

part_t* mt_get_part(const char* name) {
    return ((part_t* (*)(const char*))(0x4BD1FD24|1))(name);
}

int is_key_pressed(int key) {
    return ((int (*)(int))(0x4BD11068|1))(key);
}

void cmd_help(const char *arg, void *data, unsigned sz) {
    struct fastboot_cmd *cmd = (struct fastboot_cmd *)0x4BD731EC;

    if (!cmd) {
        fastboot_fail("No commands found!");
        return;
    }

    fastboot_info("Available oem commands:");
    while (cmd) {
        if (cmd->prefix) {
            if (strncmp(cmd->prefix, "oem", 3) == 0) {
                fastboot_info(cmd->prefix);
            }
        }
        cmd = cmd->next;
    }

    fastboot_okay("");
}

void cmd_set_bootmode(const char* arg, void* data, unsigned sz) {
    const char *mode = arg + 1;

    if (!arg) {
        fastboot_fail("No bootmode specified!");
        return;
    }

    if (strcmp(mode, "normal") == 0) {
        set_bootmode(BOOTMODE_NORMAL);
    } else if (strcmp(mode, "recovery") == 0) {
        set_bootmode(BOOTMODE_RECOVERY);
    } else if (strcmp(mode, "fastboot") == 0) {
        set_bootmode(BOOTMODE_FASTBOOT);
    } else {
        fastboot_fail("Invalid bootmode specified!");
        return;
    }

    fastboot_info("Bootmode set successfully.");
    fastboot_okay("");
}

void cmd_reboot_recovery(const char* arg, void* data, unsigned sz) {
    struct misc_message misc_msg = {0};
    strncpy(misc_msg.command, "boot-recovery", 31);

    struct device_t* dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        fastboot_fail("Block device not initialized!");
        return;
    }

    part_t* part = mt_get_part("MISC");
    if (!part) {
        fastboot_fail("MISC partition not found!");
        return;
    }

    uint64_t part_offset = (part->start_sect * BLOCK_SIZE);
    if (dev->write(dev, &misc_msg, part_offset, sizeof(misc_msg), part->part_id) < 0) {
        fastboot_fail("Failed to write bootloader message!");
        return;
    }

    fastboot_info("The device will reboot into recovery mode...");
    fastboot_okay("");
    mtk_wdt_reset();
}

void board_early_init(void) {
    printf("Entering early init for Echo Show 8 1st Gen (2019)\n");

    // Amazon uses IDME to check whether the device is locked or unlocked.
    // These functions verify if the unlock code matches the one stored in
    // the IDME partition and perform additional unlock verification checks.
    //
    // We patch them to bypass security checks and allow unrestricted fastboot
    // access regardless of the actual device lock state.
    FORCE_RETURN(0x4BD01EF4, 1);

    // This function is called by the unlock verification above to validate the
    // actual unlock code against the IDME data. However, it also gets called
    // from other parts of the bootloader, so we need to patch it as well to avoid
    // any conflicts or security check failures.
    //
    // When verification fails, it returns -1, so we patch it to always return
    // 0 (success) to ensure consistent unlock behavior throughout the system.
    FORCE_RETURN(0x4BD02068, 0);

    // The following patches disable specific video_printf calls that display
    // mode-specific messages during boot. These default messages can be
    // confusing to end users and don't provide useful information.
    //
    // We disable them here and will display our own custom messages in
    // board_late_init instead..
    NOP(0x4BD2829E, 2); // => FASTBOOT mode...
    NOP(0x4BD11FA2, 2); // => FASTBOOT mode...
    NOP(0x4BD2827C, 2); // => FACTORYRESET mode...
}

void board_late_init(void) {
    printf("Entering late init for Echo Show 8 1st Gen (2019)\n");

    // If the privacy button is held during boot, force the device into
    // FASTBOOT mode to allow easy access for debugging or flashing.
    if (is_key_pressed(KEY_PRIVACY)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        // Register our custom command(s)
        fastboot_register("oem help", cmd_help, 1);
        fastboot_register("oem set-bootmode", cmd_set_bootmode, 1);
        fastboot_register("oem reboot-recovery", cmd_reboot_recovery, 1);
        video_set_cursor(video_get_rows() / 12, 0);
    }

    if (get_bootmode() != BOOTMODE_NORMAL) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        //
        // Displaying the boot mode can be helpful for developers, as it provides
        // immediate feedback and can prevent debugging headaches.
        show_bootmode(get_bootmode());
    }
}