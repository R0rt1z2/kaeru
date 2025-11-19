//
// SPDX-FileCopyrightText: 2025 Shomy 🍂, Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include <board_ops.h>
#include "include/lamu.h"

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

    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, CMD_FLASH_PATTERN);
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

    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x43F0, 0x4604, 0x4DB1);
    if (addr) {
        printf("Found cmd_erase at 0x%08X\n", addr);
        ((void (*)(const char* arg, void* data, unsigned sz))(addr | 1))(arg, data, sz);
    } else {
        fastboot_fail("Unable to find original cmd_erase");
    }
}


int set_env(char *name, char *value) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, SET_ENV_PATTERN);
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
            0x6003,  // str r3, [r0,#0]
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

    // AVB adds device state info to the kernel cmdline, but it keeps showing
    // "unlocked" even when we want it to say "locked". This patch forces
    // the cmdline to always use the "locked" string instead of checking
    // the actual device state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4691, 0xF102);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);
        
        // Find where in libavb the device state is first fetched and then stored,
        // then Nop out the code that checks the actual device state.
        // This forces libavb to always use the "locked" string.
        NOP(addr + 0x9C, 4);
    }
}

void board_early_init(void) {
    printf("Entering early init for Motorola G15 / G05 / E15\n");

    uint32_t addr = 0;

    // The environment area is not yet initialized when board_early_init runs,
    // so environment variable checks will always return NULL at this stage.
    // To work around this timing issue, we hook into a printf call that executes
    // after environment initialization is complete and redirect it to our
    // spoof_lock_state function.
    addr = SEARCH_PATTERN(LK_START, LK_END, ENV_INIT_DONE_PATTERN);
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
    addr = SEARCH_PATTERN(LK_START, LK_END, CMD_FLASH_REGISTER_PATTERN);
    if (addr) {
        printf("Found cmd_flash_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, CMD_ERASE_REGISTER_PATTERN);
    if (addr) {
        printf("Found cmd_erase_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // Disables the `fastboot flashing lock` command to prevent accidental hard bricks.
    //
    // Locking while running a custom or modified LK image can leave the device in an
    // unbootable state after reboot, since the expected secure environment is no longer
    // present.
    addr = SEARCH_PATTERN(LK_START, LK_END, CMD_FLASHING_LOCK_REGISTER_PATTERN);
    if (addr) {
        printf("Found cmd_flashing_lock_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // Tinno (the ODM) added a post-`app()` check that forcefully relocks the device
    // if it was previously unlocked, completely defeating the purpose of unlocking.
    //
    // Fortunately, they don’t verify LK integrity, so we can bypass this check entirely
    // by patching the function to return immediately before it does anything.
    addr = SEARCH_PATTERN(LK_START, LK_END, TINNO_COMMERCIAL_LOCK_PATTERN);
    if (addr) {
        printf("Found tinno_commercial_device_force_lock at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // This functions work alongside `get_hw_sbc` to determine the device's secure state.
    // It's tied to logic that can trigger an automatic bootloader relock.
    //
    // To prevent unintended relocks, we patch it to always return a non-secure state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFFF9, 0xB908);
    if (addr) {
        printf("Found lock function at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B05, 0x447B, 0x681B, 0x681B);
    if (addr) {
        printf("Found lock function at 0x%08X\n", addr);
        FORCE_RETURN(addr, 1);
    }

    // This function is used extensively throughout the fastboot implementation to
    // determine whether the device is a development unit. Until this check passes,
    // fastboot treats the device as locked, even if the bootloader is already unlocked.
    //
    // To ensure flashing works as expected, we patch this function to always return 0,
    // effectively marking the device as non-secure.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2360, 0xF2C1, 0x13CE, 0x6818);
    if (addr) {
        printf("Found get_hw_sbc at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // This function updates the oem_mfd partition, which is later used to
    // decide whether the device should boot into factory mode, even if the user
    // requested a normal boot.
    //
    // The update is triggered based on a hardware register (likely a secure fuse
    // or similar). If that register isn't set to 1, which won't happen after
    // patching get_hw_sbc, the bootloader assumes the device is insecure and
    // repeatedly forces factory mode.
    //
    // To prevent this loop, we disable the function entirely.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0xB084, 0x4D28);
    if (addr) {
        printf("Found tinno_facmode_init at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Register our custom flash and erase commands to replace the original ones.
    fastboot_register("flash:", cmd_flash, 1);
    fastboot_register("erase:", cmd_erase, 1);
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);
}

void board_late_init(void) {
    printf("Entering late init for Motorola G15 / G05 / E15\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // As an extra safeguard, we manually disable factory mode by calling the
    // relevant update function directly. This ensures the mode is turned off,
    // even if it was set by another part of the bootloader.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x43F0, 0x460F, 0x4C1C);
    if (addr) {
        printf("Found tinno_facmode_update at 0x%08X\n", addr);
        ((int (*)(int, int))(addr | 1))(0, 0);
    }
}
