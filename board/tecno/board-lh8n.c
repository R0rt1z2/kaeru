//
// SPDX-FileCopyrightText: 2026 naden01 <naden.irsyad01@gmail.com>
// SPDX-FileCopyrightText: 2026 KanagawaYamada <albert.wesley.dion@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>
#include <lib/fastboot.h>

void board_early_init(void) {}

static void video_clean_screen(void) {
    ((void (*)(void))(0x4823cb74 | 1))();
}

static void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x4823ca20 | 1))(row, col);
}

static void mt_disp_update(void) {
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(0x48200e8c | 1))(0, 0, 1080, 2460);
}

static void cmd_uca(const char* arg, void* data, unsigned sz) {
    // 1. Send text to the PC Terminal over USB (Synced with phone screen)
    fastboot_info("Petualang pemberani penakluk dongeng,");
    fastboot_info("seorang elf vampir yang gelap namun bercahaya.");
    fastboot_info("");
    fastboot_info("Halo! aku Akayuki Mikouca yang akan");
    fastboot_info("menemanimu dari kegelapan menuju cahaya.");
    fastboot_info("");
    fastboot_info("Baru disini? Kenalan Yuk!");
    fastboot_info("");
    fastboot_info("YT: @ayamikouca");

    // 2. Render text directly onto the phone's physical screen
    video_clean_screen();
    video_set_cursor(10, 0); // Start at row 10
    video_printf("Petualang pemberani penakluk dongeng,\n");
    video_printf("seorang elf vampir yang gelap namun bercahaya.\n\n");
    video_printf("Halo! aku Akayuki Mikouca yang akan\n");
    video_printf("menemanimu dari kegelapan menuju cahaya.\n\n");
    video_printf("Baru disini? Kenalan Yuk!\n\n");
    video_printf("YT: @ayamikouca\n");
    mt_disp_update(); // Push framebuffer to display hardware

    fastboot_okay("");
}

static void cmd_yamada(const char* arg, void* data, unsigned sz) {
    // 1. Send text to the PC Terminal over USB (Synced with phone screen)
    fastboot_info("1 + 1 = 2");
    fastboot_info("");
    fastboot_info("Hi, my name is Kanagawa Yamada.");
    fastboot_info("You can call me Yamada,");
    fastboot_info("VTeacher from Indonesia");
    fastboot_info("");
    fastboot_info("YT: @KanagawaYamada");

    // 2. Render text directly onto the phone's physical screen
    video_clean_screen();
    video_set_cursor(10, 0); // Start at row 10
    video_printf("1 + 1 = 2\n\n");
    video_printf("Hi, my name is Kanagawa Yamada.\n");
    video_printf("You can call me Yamada,\n");
    video_printf("VTeacher from Indonesia\n\n");
    video_printf("YT: @KanagawaYamada\n");
    mt_disp_update(); // Push framebuffer to display hardware

    fastboot_okay("");
}

void board_late_init(void) {
    // Register custom fastboot commands
    fastboot_register("oem uca", cmd_uca, 1);
    fastboot_register("oem yamada", cmd_yamada, 1);

    // ---------------------------------------------------------
    // DYNAMIC PATCHING (OTA SURVIVABLE)
    // ---------------------------------------------------------
    
    // Disable Orange State Warning dynamically
    uint32_t orange_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                          0xb508, 0xf7ea, 0xff7f, 0xf7ea, 0xff77, 0x2100);
    if (orange_addr) {
        FORCE_RETURN(orange_addr, 0);
    }

    bootmode_t mode = get_bootmode();

    // Hardware Volume Key Boot Mode Overrides
    if (mode == BOOTMODE_RECOVERY) {
        // Vol+ detected -> Force Bootloader (Fastboot)
        set_bootmode(BOOTMODE_FASTBOOT);
        video_printf("\n>>> VOL+ DETECTED: FORCING BOOTLOADER <<<\n\n");
    }

    // Refresh mode in case it was modified
    mode = get_bootmode();

    if (mode != BOOTMODE_RECOVERY) {
        // Dynamically search for the Green State check and spoof it
        uint32_t green_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                             0x4b18, 0x447b, 0x681b, 0x681b, 0x2b03, 0xd807);
        if (green_addr) {
            WRITE16(green_addr + 6, 0x2300); // Spoof boot state to green
        }

        // Dynamically search for the VBMeta device_state generator and spoof it
        uint32_t lock_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                            0x2801, 0xd0f5, 0xb9a0, 0x9b09);
        if (lock_addr) {
            WRITE16(lock_addr + 8, 0xBF00);  // Spoof VBMeta to locked
        }
    }
}
