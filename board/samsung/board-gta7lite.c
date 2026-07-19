//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_DOWN 17

static void do_download(int mode) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB5F0, 0x4604, 0xB087, 0x2500);
    if (addr)
        ((void (*)(int))(addr | 1))(mode);
}

static void video_clean_screen(void) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B0A, 0x2100, 0xB510, 0x447B);
    if (addr)
        ((void (*)(void))(addr | 1))();
}

static void enter_fastboot(void) {
    video_clean_screen();
    set_bootmode(BOOTMODE_FASTBOOT);
    show_bootmode(BOOTMODE_FASTBOOT);
    do_download(8);
}

static long partition_read(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
        part_name, offset, data, size);
}

static long partition_write(const char* part_name, long long offset, uint8_t* data, size_t size) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xB085, 0x461F,
                                   0x4616, 0x9003, 0x9D0F, 0xF7FE, 0xFD9D, 0x1C43);
    if (addr)
        return ((long (*)(const char*, long long, uint8_t*, size_t))(addr | 1))(
            part_name, offset, data, size);
    return -1;
}

void cmd_reboot_recovery(const char* arg, void* data, unsigned sz) {
    struct misc_message misc_msg = {0};
    strncpy(misc_msg.command, "boot-recovery", 31);
    
    if (partition_write("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg)) < 0) {
        fastboot_fail("Failed to write bootloader message!");
        return;
    }
    
    fastboot_info("The device will reboot into recovery mode...");
    fastboot_okay("");
    mtk_wdt_reset();
}

void cmd_reboot_fastboot(const char* arg, void* data, unsigned sz) {
    struct misc_message misc_msg = {0};
    strncpy(misc_msg.command, "boot-fastboot", 31);
    
    if (partition_write("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg)) < 0) {
        fastboot_fail("Failed to write bootloader message!");
        return;
    }
    
    fastboot_info("The device will reboot into fastboot mode...");
    fastboot_okay("");
    mtk_wdt_reset();
}

void parse_bootloader_messages(void) {
    struct misc_message misc_msg = {0};

    if (partition_read("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg)) < 0) {
        printf("Failed to read misc partition\n");
        return;
    }

    bootmode_t mode = misc_command_to_bootmode(misc_msg.command);
    if (mode == BOOTMODE_NORMAL)
        return;

    printf("Found '%s', forcing %s\n", misc_msg.command, bootmode2str(mode));

    // Clear the command so the next boot does not loop back into this mode.
    memset(&misc_msg, 0, sizeof(misc_msg));
    partition_write("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg));

    if (mode == BOOTMODE_FASTBOOT)
        enter_fastboot();
    else
        set_bootmode(mode);
}

void board_early_init(void) {
    printf("Entering early init for Galaxy Tab A7 Lite\n");

    uint32_t addr = 0;

    // On unlocked devices, LK shows an orange state warning during boot
    // that also introduces an unnecessary 5 second delay. Forcing the
    // function to return 0 skips both the warning and the delay.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B12, 0x447B, 0x681B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // This is the helper behind the CURRENT BINARY line in download mode.
    // Samsung LK treats a zero return as "Samsung Official", so forcing
    // it to always return 0 keeps the device in the official state and
    // avoids the custom-binary warning on boot.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4813, 0xB538, 0x4478, 0xF7FF);
    if (addr) {
        printf("Found current_binary_state at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // NOP out the warranty-bit logging path so LK stops calling into the
    // code that prints the noisy "Set Warranty Bit" / "Didn't Set Warranty
    // Bit(TEST BIT)" messages on every boot.
    NOP(0x48024526, 2);
    NOP(0x480244EA, 2);

    // Force the warranty bit to read as not blown, everywhere it's checked.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0xB082, 0x2006, 0xA901, 0xF086);
    if (addr) {
        printf("Found get_warranty_fuse at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Force the unsigned-binary flash counter to 0.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4C06, 0xF7FF, 0xFE40);
    if (addr) {
        printf("Found get_unsigned_dl_count at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Make the binary signature check always pass so unsigned images flash.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0xB082, 0x4615);
    if (addr) {
        printf("Found check_secure_download at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Report download as allowed so the "MDM MODE. CAN'T DOWNLOAD" path
    // never triggers.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B04, 0x447B, 0x681B, 0x6818);
    if (addr) {
        printf("Found mdm_download_allowed at 0x%08X\n", addr);
        FORCE_RETURN(addr, 1);
    }

    // The last two gates sit inside nand_write and reuse generic helpers, so
    // patch them in place off the function entry. First skip the "partition
    // is not allowed download" reject, then let the KG-lock check fall
    // straight through to the flash.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xF6AD, 0x0D3C);
    if (addr) {
        printf("Found nand_write at 0x%08X\n", addr);
        NOP(addr + 0x4C, 2);
        PATCH_BRANCH(addr + 0x288, addr + 0xC6);
    }

    // Register our custom fastboot commands.
    fastboot_register("reboot-recovery", cmd_reboot_recovery, 1);
    fastboot_register("oem reboot-recovery", cmd_reboot_recovery, 1);
    fastboot_register("reboot-fastboot", cmd_reboot_fastboot, 1);
}

void board_late_init(void) {
    printf("Entering late init for Galaxy Tab A7 Lite\n");

    // Act on any boot command left in misc before anything else, so a
    // reboot into fastboot or recovery is honored.
    parse_bootloader_messages();

    // Samsung LK supports fastboot mode, but it is hidden by default here.
    // We repurpose Volume Down to force fastboot mode at boot, except when
    // the device is resuming from LPM, where fastboot should not be allowed.
    if (mtk_detect_key(VOLUME_DOWN) && get_bootmode() != 8)
        enter_fastboot();
}