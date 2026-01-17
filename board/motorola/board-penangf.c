//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
//                         2025 Shomy 🍂 <shomy@shomy.is-a.dev>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

long partition_read(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
}

long partition_write(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(0x4c45877c | 1))(
            part_name, offset, data, size);
}

void cmd_help(const char *arg, void *data, unsigned sz) {
    // TODO: Find a better way to get the command list
    struct fastboot_cmd *cmd = (struct fastboot_cmd *)0x4C5B2144;

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

int is_partition_protected(const char* partition) {
    if (!partition || *partition == '\0') return 1;

    while (*partition && ISSPACE(*partition)) {
        partition++;
    }

    if (*partition == '\0') return 1;

    // These partitions are critical—flashing them incorrectly can lead to a hard brick.
    // To prevent accidental damage, we mark them as protected and block write access.
    if (strcmp(partition, "boot0") == 0 || strcmp(partition, "boot1") == 0 ||
        strcmp(partition, "boot2") == 0 || strcmp(partition, "partition") == 0 ||
        strcmp(partition, "preloader") == 0 || strcmp(partition, "preloader_a") == 0 ||
        strcmp(partition, "preloader_b") == 0) {
        return 1;
    }

    return 0;
}

void cmd_flash(const char* arg, void* data, unsigned sz) {
    if (is_partition_protected(arg)) {
        fastboot_fail("Partition is protected");
        return;
    }

    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB5F8, 0x460E, 0x4994, 0x4615);
    if (addr) {
        printf("Found cmd_flash at 0x%08X\n", addr);
        ((void (*)(const char* arg, void* data, unsigned sz))(addr | 1))(arg, data, sz);
    } else {
        fastboot_fail("Unable to find original cmd_flash");
    }
}

void cmd_erase(const char* arg, void* data, unsigned sz) {
    if (is_partition_protected(arg)) {
        fastboot_fail("Partition is protected");
        return;
    }

    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4604, 0x4878);
    if (addr) {
        printf("Found cmd_erase at 0x%08X\n", addr);
        ((void (*)(const char* arg, void* data, unsigned sz))(addr | 1))(arg, data, sz);
    } else {
        fastboot_fail("Unable to find original cmd_erase");
    }
}

int set_env(char *name, char *value) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x2200, 0xF7FF, 0xBF0D, 0xBF00);
    if (addr) {
        printf("Found set_env at 0x%08X\n", addr);
        return ((int (*)(char *name, char *value))(addr | 1))(name, value);
    }
    return -1;
}

char *get_env(char *name) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4602, 0x4604, 0x4909);
    if (addr) {
        printf("Found get_env at 0x%08X\n", addr);
        return ((char* (*)(char *name))(addr | 1))(name);
    }
    return NULL;
}

void parse_bootloader_messages(void) {
    struct misc_message misc_msg = {0};

    if (partition_read("misc", 0, (uint8_t *)&misc_msg, sizeof(misc_msg)) < 0) {
        printf("Failed to read misc partition\n");
        return;
    }

    printf("Read bootloader command: %s\n", misc_msg.command);

    if (strncmp(misc_msg.command, "boot-recovery", 13) == 0) {
        printf("Found boot-recovery, forcing recovery\n");
        set_bootmode(BOOTMODE_RECOVERY);
        memset(&misc_msg, 0, sizeof(misc_msg));
        partition_write("misc", 0, (uint8_t *)&misc_msg, sizeof(misc_msg));
    }
    else if (strncmp(misc_msg.command, "boot-bootloader", 15) == 0) {
        printf("Found boot-bootloader, forcing fastboot\n");
        set_bootmode(BOOTMODE_FASTBOOT);
        memset(&misc_msg, 0, sizeof(misc_msg));
        partition_write("misc", 0, (uint8_t *)&misc_msg, sizeof(misc_msg));
    }
}

void cmd_spoof_bootloader_lock(const char* arg, void* data, unsigned sz) {
    uint32_t status = 0;
    const char* env_value = get_env(KAERU_ENV_BLDR_SPOOF);
    const char *option = arg + 1;
    status = (env_value && strcmp(env_value, "1") == 0) ? 1 : 0;

    if (option) {
        if (!strcmp(option, "off")) {
            if (status) {
                set_env(KAERU_ENV_BLDR_SPOOF, "0");
                fastboot_publish("is-spoofing", "0");
                fastboot_info("Bootloader spoofing disabled.");
                fastboot_info("A factory reset may be required.");
            } else {
                fastboot_info("Bootloader spoofing is already disabled.");
            }
            fastboot_okay("");
            return;
        }

        if (!strcmp(option, "on")) {
            if (!status) {
                set_env(KAERU_ENV_BLDR_SPOOF, "1");
                fastboot_publish("is-spoofing", "1");
                fastboot_info("Bootloader spoofing enabled.");
                fastboot_info("A factory reset may be required.");
            } else {
                fastboot_info("Bootloader spoofing is already enabled.");
            }
            fastboot_okay("");
            return;
        }

        if (!strcmp(option, "status")) {
            fastboot_info(status ?
                "Bootloader spoofing is currently enabled." :
                "Bootloader spoofing is currently disabled.");
            fastboot_info(status ?
                "Device is currently spoofed as bootloader locked." :
                "Device is not being spoofed as bootloader locked.");
            fastboot_okay("");
            return;
        }
    }


    fastboot_info("kaeru bootloader lock spoofing control");
    fastboot_info("");
    fastboot_info("When enabled, device reports as 'locked' to TEE");
    fastboot_info("while maintaining full fastboot and root capabilities.");
    fastboot_info("");
    fastboot_info("Commands:");
    fastboot_info("  on     - Enable spoofing (reboot required)");
    fastboot_info("  off    - Disable spoofing (reboot required)");
    fastboot_info("  status - Show current state");
    fastboot_fail("Usage: fastboot oem bldr_spoof <on|off|status>");
}

void spoof_lock_state(void) {
    uint32_t addr = 0;

    // Put the bootloader spoofing patches behind an env flag to make it optional,
    // as well as allowing toggling (needed for testing or GSI)
    char* env_value = get_env(KAERU_ENV_BLDR_SPOOF);
    if (env_value && strcmp(env_value, "1") == 0) {
        printf("Bootloader lock status spoofing enabled, applying patches.\n");
        fastboot_publish("is-spoofing", "1");
    } else {
        printf("Bootloader lock status spoofing disabled.\n");
        fastboot_publish("is-spoofing", "0");
        return;
    }

    // Need to spoof the LKS_STATE as "locked" for certain scenarios, but still
    // return success so other parts of the system don't freak out. This makes
    // seccfg_get_lock_state always report lock_state=1 and return 2.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);
    if (addr) {
        printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
        PATCH_MEM(addr + 6, 
            0x2301,  // movs r3, #1
            0x6023,  // str r3, [r4, #0]
            0x2002,  // movs r0, #2
            0xbd10   // pop {r4, pc}
        );
    }

    // Force the secure boot state to ATTR_SBOOT_ENABLE (0x11). This controls whether
    // secure boot verification is enabled and is separate from the LKS_STATE above.
    // Setting it to 0x11 indicates secure boot is properly enabled.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4604, 0x2001, 0xF7FF);
    if (addr) {
        printf("Found get_sboot_state at 0x%08X\n", addr);
        PATCH_MEM(addr,
            0x2311,  // movs r3, #0x11
            0x0360,  // str r3, [r0,#0]
            0x2000,  // movs r0, #0
            0x4770   // bx lr
        );
    }

    // When we spoof the lock state to appear "locked", fastboot starts rejecting 
    // commands with "not support on security" and "not allowed in locked state" 
    // errors. This is annoying since the device is actually unlocked underneath, 
    // the security checks are just being overly paranoid.
    //
    // This patch removes both security gates so fastboot commands work regardless
    // of what the spoofed lock state reports.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4880, 0xB087, 0x4D5A);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);
        
        // NOP the error message calls
        NOP(addr + 0x15A, 2);  // "not support on security" call
        NOP(addr + 0x166, 2);  // "not allowed in locked state" call
        
        // Jump directly to command handler
        PATCH_MEM(addr + 0xF0, 0xE006);  // b +12 (branch to command handler)
    }

    // Since we're spoofing the LKS_STATE as locked, get_vfy_policy would normally
    // require partition verification during boot. Force it to return 0 to disable
    // this verification and allow booting with modified partitions.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF63, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Since we're spoofing the LKS_STATE as locked, get_dl_policy would normally
    // restrict fastboot downloads/flashing based on security policy. Force it to 
    // return 0 to bypass these restrictions and allow unrestricted flashing.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF5D, 0xF000);
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // AVB adds device state info to the kernel cmdline, but it keeps showing
    // "unlocked" even when we want it to say "locked". This patch forces
    // the cmdline to always use the "locked" string instead of checking
    // the actual device state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4691, 0xF102);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);
        
        // Find the location where it checks local_ac[0] and force it to use "locked"
        // Around offset +0xA8 is where it loads the "locked"/"unlocked" strings
        // Force it to always load "locked" string
        PATCH_MEM(addr + 0x9C, 0x4A91, 0x7A44);
    }
}

void board_early_init(void) {
    printf("Entering early init for Motorola G13 / G23\n");

    uint32_t addr = 0;

    // The environment area is not yet initialized when board_early_init runs,
    // so environment variable checks will always return NULL at this stage.
    // To work around this timing issue, we hook into a printf call that executes
    // after environment initialization is complete and redirect it to our
    // spoof_lock_state function.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF035, 0xFEC7, 0x6823, 0x2000);
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void*)spoof_lock_state, TARGET_THUMB);
    }

    // The default flash and erase commands perform no safety checks, allowing
    // writes to critical partitions, like the Preloader, which can easily brick
    // the device.
    //
    // To prevent this, we disable the original handlers and replace them with
    // custom wrappers that verify whether the target partition is protected.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7FF, 0xFA72, 0xF8DF, 0x1670);
    if (addr) {
        printf("Found cmd_flash_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7FF, 0xFA68, 0xF8DF, 0x1664);
    if (addr) {
        printf("Found cmd_erase_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // Disables the `fastboot flashing lock` command to prevent accidental hard bricks.
    //
    // Locking while running a custom or modified LK image can leave the device in an
    // unbootable state after reboot, since the expected secure environment is no longer
    // present.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7FF, 0xF929, 0xF8DF, 0x14D4);
    if (addr) {
        printf("Found cmd_flashing_lock_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // Register our custom flash and erase commands to replace the original ones.
    fastboot_register("flash:", cmd_flash, 1);
    fastboot_register("erase:", cmd_erase, 1);
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);
    fastboot_register("oem help", cmd_help, 1);
}

void board_late_init(void) {
    printf("Entering late init for Motorola G13 / G23\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B, 0x681B, 0x681B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // By default, the stock LK restricts flashing to slot A only.
    // Attempts to flash slot B result in a vague error message, despite no
    // technical reason for the limitation.
    //
    // This patch removes the restriction, enabling flashing to both A/B slots,
    // which should have been the default behavior to begin with.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xD170, 0x4852, 0x4478, 0xF003);
    if (addr) {
        printf("Found no_permit_flash at 0x%08X\n", addr);
        NOP(addr, 1);
    }

    // Disables the boot menu that appears when entering fastboot mode.
    //
    // If Volume Down is held, the menu auto-selects and executes an option—often
    // unintentionally. This patch removes the function call responsible for
    // displaying the menu, keeping fastboot mode clean and predictable.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF022, 0xFE0C, 0xF8DF, 0x0424);
    if (addr) {
        printf("Found fastboot_menu_select at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // The stock bootloader ignores boot commands written to the misc partition,
    // making it impossible to programmatically reboot into fastboot or recovery.
    // We implement our own misc parsing so tools like mtkclient or Penumbra can
    // trigger these modes automatically by writing to misc before rebooting.
    parse_bootloader_messages();

    // The stock bootloader has the worst key combo handling I've ever seen.
    // It works whenever it feels like it, making it a nightmare to enter
    // recovery or fastboot mode through key combos.
    //
    // This patch restores expected behavior:
    // - Volume Up → Recovery
    // - Volume Down → Fastboot
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
    } else if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL 
        && mode != BOOTMODE_POWEROFF_CHARGING &&  !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        show_bootmode(mode);
    }

#if KAERU_DEBUG
    // Redirect "lk finished" message from printf to video_printf to ensure we
    // got past lk. Useful for debugging ROMS and kernels, and to know if
    // the kernel is even being loaded.
    PATCH_CALL(0x4C42AD30, (void*)CONFIG_VIDEO_PRINTF_ADDRESS, TARGET_THUMB);
#endif
}