//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#include <wdt/mtk_wdt.h>

#define VOLUME_UP 1

long partition_read(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
}

long partition_write(const char* part_name, long long offset, uint8_t* data, size_t size) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xB085, 0x461F, 0x4616,
                                                     0x9003, 0x9D0F, 0xF7FE, 0xFD59);
    if (addr)
        return ((long (*)(const char*, long long, uint8_t*, size_t))(addr | 1))(
            part_name, offset, data, size);
    return -1;
}

// Fastboot is only reachable through ramdump mode, whose flag also makes
// the dispatcher reject anything but 'oem mrdump'. NOP the branch that
// picks LG's boot path to get there without it.
static void force_fastboot(void) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xF005, 0xF803, 0xB1F8, 0xF7CC);
    if (addr) {
        printf("Found ramdump mode check at 0x%08X\n", addr + 4);
        NOP(addr + 4, 1);
        show_bootmode(BOOTMODE_FASTBOOT);
    }
}

// The stock bootloader ignores boot commands written to the misc partition,
// so parse them ourselves and let tools pick the boot mode.
static bool parse_bootloader_messages(void) {
    struct misc_message misc_msg = {0};

    if (partition_read("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg)) < 0) {
        printf("Failed to read misc partition\n");
        return false;
    }

    bootmode_t mode = misc_command_to_bootmode(misc_msg.command);
    if (mode == BOOTMODE_NORMAL)
        return false;

    printf("Found '%s', forcing %s\n", misc_msg.command, bootmode2str(mode));

    memset(&misc_msg, 0, sizeof(misc_msg));
    partition_write("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg));

    // Fastboot isn't a boot mode here, it's the ramdump branch patch. LK picks
    // the rest up from the global on its own, so let the caller know which.
    if (mode == BOOTMODE_FASTBOOT)
        return true;

    set_bootmode(mode);
    show_bootmode(mode);

    return false;
}

static void cmd_reboot(const char* arg, void* data, unsigned sz) {
    const char* command = reboot_target_to_misc_command(arg);

    if (!command && arg && arg[0]) {
        fastboot_fail("unknown reboot target");
        return;
    }

    if (command) {
        struct misc_message misc_msg = {0};

        strncpy(misc_msg.command, command, sizeof(misc_msg.command) - 1);

        if (partition_write("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg)) < 0) {
            fastboot_fail("failed to write misc");
            return;
        }
    }

    fastboot_okay("");
    mtk_wdt_reset();
}

void board_early_init(void) {
    printf("Entering early init for LG Velvet 5G\n");
}

void board_late_init(void) {
    printf("Entering late init for LG Velvet 5G\n");

    uint32_t addr = 0;

    if (parse_bootloader_messages() || mtk_detect_key(VOLUME_UP))
        force_fastboot();

    // Drop every stock reboot registration and take the whole family over with
    // a single 'reboot', which is what the dispatcher would match first anyway.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x5879, 0x4478, 0xF7FF, 0xF9AE);
    if (addr) {
        printf("Found reboot registrations at 0x%08X\n", addr + 4);
        NOP(addr + 0x04, 2); // reboot
        NOP(addr + 0x18, 2); // reboot-bootloader
        NOP(addr + 0x2C, 2); // reboot-recovery
        NOP(addr + 0x40, 2); // reboot-fastboot
        fastboot_register("reboot", cmd_reboot, 1);
    }

    // The verified boot warning branches on state: 2 is orange, 3 red, 1
    // yellow. NOP the orange branch so it falls through the other two and
    // returns without ever drawing the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2B02, 0xD052, 0x2B03, 0xD03A);
    if (addr) {
        printf("Found orange state warning at 0x%08X\n", addr + 2);
        NOP(addr + 2, 1);
    }
}
