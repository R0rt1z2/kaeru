//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

int set_env(char *name, char *value) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x2200, 0xF7FF, 0xBF0D, 0xBF00, 0xE92D);
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
    printf("Entering early init for Motorola G06\n");

    uint32_t addr = 0;

    // The environment area is not yet initialized when board_early_init runs,
    // so environment variable checks will always return NULL at this stage.
    // To work around this timing issue, we hook into a printf call that executes
    // after environment initialization is complete and redirect it to our
    // spoof_lock_state function.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF03F, 0xFC3A, 0x6823, 0x2000);
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void*)spoof_lock_state, TARGET_THUMB);
    }

    // Ontim (the ODM) added a post-`app()` check that forcefully relocks the device
    // if it was previously unlocked, completely defeating the purpose of unlocking.
    //
    // Fortunately, they don’t verify LK integrity, so we can bypass this check entirely
    // by patching the function to return immediately before it does anything.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0xF05D, 0xFA59, 0xB160);
    if (addr) {
        printf("Found lock function at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
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

    // This function is used to check whether factory mode is enabled or not. When the
    // bootloader is unlocked, it gets automatically enabled on every boot, but this is
    // not ideal for us since we'll get a nasty watermark on the screen when the OS -boots.
    //
    // Simply force the getter to always return 0, disabling factory mode entirely.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B05, 0x447B, 0x681B, 0x681B);
    if (addr) {
        printf("Found get_facmode at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Register our custom fastboot commands.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);
}

void board_late_init(void) {
    printf("Entering late init for Motorola G06\n");

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
}
