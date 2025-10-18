//
// SPDX-FileCopyrightText: 2025 Shomy 🍂, Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include <board_ops.h>

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

    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB5F8, 0x4605, 0x4C60, 0x460F);
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

void board_early_init(void) {
    printf("Entering early init for Motorola G15 / G05\n");

    uint32_t addr = 0;

    // The default flash and erase commands perform no safety checks, allowing
    // writes to critical partitions, like the Preloader, which can easily brick
    // the device.
    //
    // To prevent this, we disable the original handlers and replace them with
    // custom wrappers that verify whether the target partition is protected.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7FF, 0xFA04, 0xF8DF, 0x1624);
    if (addr) {
        printf("Found cmd_flash_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7FF, 0xFAF9, 0xF8DF, 0x1618);
    if (addr) {
        printf("Found cmd_erase_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // Disables the `fastboot flashing lock` command to prevent accidental hard bricks.
    //
    // Locking while running a custom or modified LK image can leave the device in an
    // unbootable state after reboot, since the expected secure environment is no longer
    // present.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7FF, 0xF8D7, 0xF8DF, 0x14B0);
    if (addr) {
        printf("Found cmd_flashing_lock_register at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // Tinno (the ODM) added a post-`app()` check that forcefully relocks the device
    // if it was previously unlocked, completely defeating the purpose of unlocking.
    //
    // Fortunately, they don’t verify LK integrity, so we can bypass this check entirely
    // by patching the function to return immediately before it does anything.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB538, 0xF04A, 0xFBF9, 0xB110);
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
}

void board_late_init(void) {
    printf(​"Entering late init for Motorola G15 / G05\n");

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
