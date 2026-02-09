//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <me@r0rt1z2.com>
//                         2025-2026 Shomy 🍂 <shomy@shomy.is-a.dev>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1
#define POWER 8
#define CMDLINE1_ADDR 0x4C50AC6C
#define CMDLINE2_ADDR 0x4C50BC70

typedef struct {
    const char label[32];
    void (*callback)(void);
} fb_ui_option_t;

typedef enum {
    FB_OPTION_REBOOT = 0,
    FB_OPTION_BOOTLOADER,
    FB_OPTION_RECOVERY,
    FB_OPTION_FASTBOOTD,
    FB_OPTION_POWEROFF,
    FB_OPTION_CHANGE_SPOOF,
    FB_OPTION_KAERU_INFO,
    FB_OPTION_COUNT
} fb_option_idx_t;

const uint32_t fb_ui_options_count = FB_OPTION_COUNT;

fb_ui_option_t fb_ui_options[FB_OPTION_COUNT] = {
        [FB_OPTION_REBOOT] = {.label = "REBOOT", .callback = NULL},
        [FB_OPTION_BOOTLOADER] = {.label = "BOOTLOADER", .callback = NULL},
        [FB_OPTION_RECOVERY] = {.label = "RECOVERY", .callback = NULL},
        [FB_OPTION_FASTBOOTD] = {.label = "FASTOOT (USERSPACE)", .callback = NULL},
        [FB_OPTION_POWEROFF] = {.label = "POWEROFF", .callback = NULL},
        [FB_OPTION_CHANGE_SPOOF] = {.label = "CHANGE SPOOF STATUS", .callback = NULL},
        [FB_OPTION_KAERU_INFO] = {.label = "VIEW KAERU INFO", .callback = NULL},
};


char* g_spoof_status = "0";
int8_t g_fb_ui_idx = 0;

static void video_clean_screen(void) {
    ((void (*)(void))(0x4C43A4B0 | 1))();
}

static int video_get_rows(void) {
    return ((int (*)(void))(0x4C43A4A4 | 1))();
}

static void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x4C43A468 | 1))(row, col);
}

static void video_center(void) {
    video_set_cursor(video_get_rows() / 2, 0);
}

static void mt_disp_update(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(0x4C4016B8 | 1))(x, y, width, height);
}


static void mdelay(unsigned long msecs) {
    ((void (*)(unsigned long))(0x4C45D9E0 | 1))(msecs);
}


long partition_read(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
}

long partition_write(const char* part_name, long long offset, uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, uint8_t*, size_t))(0x4C45877C | 1))(
            part_name, offset, data, size);
}

void switch_spoof_status(void) {
    int new_status = !is_spoofing_enabled();

    g_spoof_status = new_status ? "1" : "0";
    set_env(KAERU_ENV_BLDR_SPOOF, g_spoof_status);
}

static void fastboot_ui_info(void) {
    const char* title = "FASTBOOT MODE (kaeru)";

    static char kaeru_version[32];
    static char spoofing_status[32];

    npf_snprintf(kaeru_version, sizeof(kaeru_version), "Kaeru version: %s", KAERU_VERSION);

    npf_snprintf(spoofing_status, sizeof(spoofing_status), "Spoofing status: %s", g_spoof_status);

    const char* lines[] = {kaeru_version, spoofing_status};

    const int num_lines = sizeof(lines) / sizeof(lines[0]);

    const uint32_t left_margin = 8;
    const uint32_t right_margin = 16;
    const uint32_t bottom_margin = 120;
    const uint32_t line_spacing = 2;
    const uint32_t title_extra_gap = 6;
    const uint32_t char_h = fb_get_char_height();

    const uint32_t title_h =
            CONFIG_FRAMEBUFFER_HEIGHT - bottom_margin - title_extra_gap - char_h * num_lines;

    uint32_t region_start_h = title_h;
    uint32_t region_height = char_h + title_extra_gap + (char_h * num_lines) +
                             (line_spacing * (num_lines - 1)) + char_h + bottom_margin;
    uint32_t region_width = CONFIG_FRAMEBUFFER_WIDTH - left_margin - right_margin;

    fb_fill_rect(left_margin, region_start_h, region_width, region_height, FB_BLACK);

    fb_text(left_margin, title_h, title, FB_RED);

    uint32_t current_h = title_h + char_h + title_extra_gap;
    for (int i = 0; i < num_lines; i++) {
        fb_text(left_margin, current_h, lines[i], FB_WHITE);
        current_h += char_h + line_spacing;
    }

    static char selected_label[64];
    npf_snprintf(selected_label, sizeof(selected_label), "Selected: %s",
                 fb_ui_options[g_fb_ui_idx].label);

    fb_text(left_margin, current_h + title_extra_gap + 30, selected_label, FB_CYAN);
}

static int fastboot_ui(void* arg) {
    video_clean_screen();
    video_center();
    fb_set_text_scale(2);
    fb_clear(FB_BLACK);

    g_fb_ui_idx = 0;
    g_spoof_status = is_spoofing_enabled() ? "1" : "0";
    fastboot_ui_info();
    mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);

    mdelay(1500);

    for (;;) {
        fastboot_ui_info();
        mt_disp_update(0, 0, CONFIG_FRAMEBUFFER_WIDTH, CONFIG_FRAMEBUFFER_HEIGHT);

        if (mtk_detect_key(VOLUME_UP)) {
            g_fb_ui_idx = (g_fb_ui_idx + fb_ui_options_count - 1) % fb_ui_options_count;
            mdelay(100);
        } else if (mtk_detect_key(VOLUME_DOWN)) {
            g_fb_ui_idx = (g_fb_ui_idx + 1) % fb_ui_options_count;
            mdelay(100);
        }


        else if (mtk_detect_key(POWER)) {
            switch (g_fb_ui_idx) {
                case FB_OPTION_REBOOT:
                    mtk_wdt_reset();
                    break;

                case FB_OPTION_BOOTLOADER:
                    ((void (*)(void))(0x4C42DD7C | 1))();
                    break;

                case FB_OPTION_RECOVERY:
                    ((void (*)(void))(0x4C42D8E4 | 1))();
                    break;

                case FB_OPTION_FASTBOOTD:
                    ((void (*)(void))(0x4C42D920 | 1))();
                    break;

                case FB_OPTION_POWEROFF:
                    ((void (*)(void))(0x4C41B9B8 | 1))();
                    break;

                case FB_OPTION_CHANGE_SPOOF:
                    switch_spoof_status();
                    break;

                case FB_OPTION_KAERU_INFO:
                    video_center();
                    print_kaeru_info(OUTPUT_VIDEO);
                    break;

                default:
                    break;
            }
            mdelay(300);
        }
        mdelay(100);
    }
    return 0;
}

void cmd_help(const char *arg, void *data, unsigned sz) {
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

    // These partitions are critical, flashing them incorrectly can lead to a hard brick.
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

static void handle_recovery_boot(void) {
    if (get_bootmode() != BOOTMODE_RECOVERY || !is_spoofing_enabled())
        return;

    printf("Recovery boot detected, modifying cmdline for unlocked state.\n");

    static const uint32_t cmdline_addrs[] = { CMDLINE1_ADDR, CMDLINE2_ADDR };
    for (int i = 0; i < ARRAY_SIZE(cmdline_addrs); i++) {
        printf("Patching cmdline at 0x%08X\n", cmdline_addrs[i]);
        cmdline_replace((char *)cmdline_addrs[i],
            "androidboot.verifiedbootstate=", "green", "orange");
    }
}

void spoof_lock_state(void) {
    uint32_t addr = 0;

    // On most MediaTek devices, lock state is fetched by calling
    // seccfg_get_lock_state() directly. Some vendors (e.g. Xiaomi)
    // add a wrapper that also checks a custom lock mechanism, but
    // this device does not have one. All callers reach
    // seccfg_get_lock_state() through a single b.w thunk.
    //
    // Rather than patching the function body directly, we redirect
    // the thunk to our own get_lock_state(), keeping the original
    // function intact while covering all call sites with a single
    // patch.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);
    if (addr) {
        printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
        PATCH_BRANCH(addr, (void*)get_lock_state);
    }

    // LK has two security gates in the fastboot command processor that
    // reject commands with "not support on security" and "not allowed
    // in locked state" errors. When spoofing lock state, these would
    // block all fastboot operations despite the device being actually
    // unlocked underneath.
    //
    // Even without spoofing, we patch these out as a safety measure
    // since OEM-specific checks could still interfere with fastboot
    // commands in unexpected ways.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4880, 0xB087, 0x4D5A);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);

        // "not support on security" call
        NOP(addr + 0x15A, 2);

        // "not allowed in locked state" call
        NOP(addr + 0x166, 2);

        // Jump directly to command handler
        PATCH_MEM(addr + 0xF0, 0xE006);
    }

    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");

    if (!spoofing) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    printf("Bootloader lock status spoofing enabled, applying patches.\n");

    // When booting into recovery, we need to ensure verifiedbootstate
    // is set to "orange" so fastbootd detects the device as unlocked
    // and allows flashing. We also patch a few other cmdline params
    // (secureboot, device_state) as a precaution in case stock
    // recovery checks them as well.
    PATCH_CALL(0x4C42A400, (void *)handle_recovery_boot, TARGET_THUMB);

    // AVB adds device state info to the kernel cmdline, but it
    // keeps showing "unlocked" even when we want it to say "locked".
    // This patch forces the cmdline to always use the "locked"
    // string instead of checking the actual device state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4691, 0xF102);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);

        // NOP out the code that checks the actual device state,
        // forcing libavb to always use the "locked" string.
        NOP(addr + 0x9C, 4);
    }

    // AVB verifies vbmeta public keys in two places: once for the main
    // vbmeta image (validate_vbmeta_public_key) and once for chained
    // vbmeta images (avb_safe_memcmp against the expected key). Both
    // reject the boot if the key doesn't match, causing the "Public key
    // used to sign data rejected" error. We patch both checks so any
    // key is accepted regardless.
    //
    // The chain key check first compares key lengths before calling
    // memcmp. If lengths differ, it skips memcmp and falls straight
    // to the error path. Change "cmp r2, r3" to "cmp r3, r3" so the
    // length check always succeeds, allowing execution to reach the
    // memcmp path (which we NOP below).
    PATCH_MEM(0x4C46181C, 0x451B);

    // NOP the bne.w that rejects mismatched chained vbmeta keys,
    // falling through to the success path unconditionally.
    NOP(0x4C461B48, 2);

    // Replace "cmp r3, #0" with "movs r3, #1" so key_is_trusted
    // is always nonzero and the following bne.w takes the success
    // branch.
    PATCH_MEM(0x4C461BBA, 0x2301);
}

void board_early_init(void) {
    printf("Entering early init for Motorola G13 / G23\n");

    uint32_t addr = 0;

    // Regardless of whether spoofing is enabled, we always need to
    // disable image authentication. The user may just be using this
    // custom LK to unlock their device, or they may be spoofing
    // where the locked state would enforce verification.
    //
    // Forcing get_vfy_policy to return 0 skips certificate
    // verification for all partitions and firmware images (boot,
    // recovery, dtbo, SCP, etc.) so the device can boot with
    // modified or unsigned images.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF63, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Same idea but for download policy, forcing get_dl_policy to return
    // 0 ensures no partition is marked as download-forbidden, so flashing
    // via fastboot works for all partitions.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF5D, 0xF000);
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
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

    // The environment area isn't initialized yet when board_early_init
    // runs, so any get_env calls would return NULL at this stage. We
    // hook a printf call in platform_init that runs right after env
    // initialization completes, it's a convenient entry point since
    // the call itself is non-essential and we need the env to be ready
    // before applying our lock state patches.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF035, 0xFEC7, 0x6823, 0x2000);
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void*)spoof_lock_state, TARGET_THUMB);
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

    // On unlocked devices, LK shows an orange state warning during boot
    // that also introduces an unnecessary 5 second delay. Forcing the
    // function to return 0 skips both the warning and the delay.
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

    if (mode == BOOTMODE_FASTBOOT) {
         thread_t* thr;
         thr = thread_create("fastboot_ui", fastboot_ui, NULL, LOW_PRIORITY, DEFAULT_STACK_SIZE);
         if (thr) thread_resume(thr);
     }


#if KAERU_DEBUG
    // Redirect "lk finished" message from printf to video_printf to ensure we
    // got past lk. Useful for debugging ROMS and kernels, and to know if
    // the kernel is even being loaded.
    PATCH_CALL(0x4C42AD30, (void*)CONFIG_VIDEO_PRINTF_ADDRESS, TARGET_THUMB);
#endif
}
