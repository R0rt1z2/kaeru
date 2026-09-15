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

static void cmd_aria(const char* arg, void* data, unsigned sz) {
    fastboot_info("After all... even though we're friends,");
    fastboot_info("Aria the idol is sure to bring you way");
    fastboot_info("more surprises down the road.");

    video_clean_screen();
    video_set_cursor(10, 0);
    video_printf("After all... even though we're friends,\n");
    video_printf("Aria the idol is sure to bring you way\n");
    video_printf("more surprises down the road.\n");
    mt_disp_update();

    fastboot_okay("");
}

static void cmd_sunna(const char* arg, void* data, unsigned sz) {
    fastboot_info("Pfft- That's not crying, Xiao'Yu!");
    fastboot_info("That's just a whole performance");
    fastboot_info("piece with your face!");

    video_clean_screen();
    video_set_cursor(10, 0);
    video_printf("Pfft- That's not crying, Xiao'Yu!\n");
    video_printf("That's just a whole performance\n");
    video_printf("piece with your face!\n");
    mt_disp_update();

    fastboot_okay("");
}

static void cmd_nangongyu(const char* arg, void* data, unsigned sz) {
    fastboot_info("It's just a meet-and-greet,");
    fastboot_info("why so nervous? Worried");
    fastboot_info("you'll fall for me?");

    video_clean_screen();
    video_set_cursor(10, 0);
    video_printf("It's just a meet-and-greet,\n");
    video_printf("why so nervous? Worried\n");
    video_printf("you'll fall for me?\n");
    mt_disp_update();

    fastboot_okay("");
}

static void cmd_aod(const char* arg, void* data, unsigned sz) {
    fastboot_info("Yume o tsukande hanasanaide");
    fastboot_info("Kogare akogare utau no");
    fastboot_info("Shibireru guzo ni koi shiteru");
    fastboot_info("Jonetsu wa kitto");
    fastboot_info("uso ni wa naranai kara");

    video_clean_screen();
    video_set_cursor(10, 0);
    video_printf("Yume o tsukande hanasanaide\n");
    video_printf("Kogare akogare utau no\n");
    video_printf("Shibireru guzo ni koi shiteru\n");
    video_printf("Jonetsu wa kitto\n");
    video_printf("uso ni wa naranai kara\n");
    mt_disp_update();

    fastboot_okay("");
}

static void cmd_yamada(const char* arg, void* data, unsigned sz) {
    fastboot_info("1 + 1 = 2");
    fastboot_info("");
    fastboot_info("Hi, my name is Kanagawa Yamada.");
    fastboot_info("You can call me Yamada,");
    fastboot_info("VTeacher from Indonesia");
    fastboot_info("");
    fastboot_info("YT: @KanagawaYamada");

    video_clean_screen();
    video_set_cursor(10, 0); 
    video_printf("1 + 1 = 2\n\n");
    video_printf("Hi, my name is Kanagawa Yamada.\n");
    video_printf("You can call me Yamada,\n");
    video_printf("VTeacher from Indonesia\n\n");
    video_printf("YT: @KanagawaYamada\n");
    mt_disp_update();

    fastboot_okay("");
}

void board_late_init(void) {
    // Register custom fastboot commands
    fastboot_register("oem aria", cmd_aria, 1);
    fastboot_register("oem sunna", cmd_sunna, 1);
    fastboot_register("oem nangongyu", cmd_nangongyu, 1);
    fastboot_register("oem aod", cmd_aod, 1);
    fastboot_register("oem yamada", cmd_yamada, 1);

    video_clean_screen();
    video_set_cursor(15, 0);
    video_printf(" Hold Your Dreams Don't Let Them Go \n");
    video_printf("====================================\n");
    video_printf("Angel of Delusion | ReDreaming Angel\n");
    mt_disp_update();

    uint32_t orange_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                          0xb508, 0xf7ea, 0xff7f, 0xf7ea, 0xff77, 0x2100);
    if (orange_addr) {
        FORCE_RETURN(orange_addr, 0);
    }

    bootmode_t mode = get_bootmode();

    if (mode == BOOTMODE_RECOVERY) {
        set_bootmode(BOOTMODE_FASTBOOT);
        video_set_cursor(40, 0); 
        video_printf("\n>>> VOL+ DETECTED: FORCING BOOTLOADER <<<\n\n");
        mt_disp_update();
    }

    mode = get_bootmode();

    if (mode != BOOTMODE_RECOVERY) {
        uint32_t green_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                             0x4b18, 0x447b, 0x681b, 0x681b, 0x2b03, 0xd807);
        if (green_addr) {
            WRITE16(green_addr + 6, 0x2300);
        }

        uint32_t lock_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                            0x2801, 0xd0f5, 0xb9a0, 0x9b09);
        if (lock_addr) {
            WRITE16(lock_addr + 8, 0xBF00);
        }
    }
}
