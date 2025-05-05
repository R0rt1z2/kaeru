//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/arm.h>
#include <board_ops.h>
#include <lib/bootmode.h>
#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/string.h>

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

void board_early_init(void) {
    printf("Entering early init for Motorola G13 / G23\n");

    uint32_t addr = 0;

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
}