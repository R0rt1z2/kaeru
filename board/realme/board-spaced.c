//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-FileCopyrightText: 2026 bulatorr <84269101+bulatorr@users.noreply.github.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>
#include <lib/thread.h>
#include <wdt/mtk_wdt.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1
#define CMDLINE1_ADDR 0x4C42E7F8
#define CMDLINE2_ADDR 0x4C53B3CC

static void mt_power_off(void) {
    ((void (*)(void))(0x4C41C608 | 1))();
}

static int video_get_rows(void) {
    return ((int (*)(void))(0x4C43E2C0 | 1))();
}

static void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x4C43E06C | 1))(row, col);
}

static void video_clean_screen(void) {
    ((void (*)(void))(0x4C43E2D8 | 1))();
}

static void mdelay(unsigned long msecs) {
    ((void (*)(unsigned long))(0x4C460708 | 1))(msecs);
}

static void cmd_reboot(void) {
    ((void (*)(void))(0x4C42DA60 | 1))();
}

static void cmd_reboot_recovery(void) {
    ((void (*)(void))(0x4C42D5F8 | 1))();
}

static void cmd_reboot_fastboot(void) {
    ((void (*)(void))(0x4C42D5F8 | 1))();
}

static void cmd_reboot_bootloader(void) {
    ((void (*)(void))(0x4C42DAAC | 1))();
}

static void mt_show_logo(void) {
    ((void (*)(void))(0x4C404C40 | 1))();
}

void cmd_shutdown(const char* arg, void* data, unsigned sz) {
    fastboot_info("The device will power off...");
    fastboot_info("Make sure to unplug the USB cable!");
    fastboot_okay("");
    mt_power_off();
}

void switch_spoof_status(void) {
    int new_status = !is_spoofing_enabled();
    char* g_spoof_status = new_status ? "1" : "0";
    set_env(KAERU_ENV_BLDR_SPOOF, g_spoof_status);
}

void update_menu(unsigned int index) {    
    const char* env_value = get_env(KAERU_ENV_BLDR_SPOOF);
    char* status = (env_value && strcmp(env_value, "1") == 0) ? " on" : "off";
    const char* title_msg = "Select Boot Mode:\n[VOLUME_UP to select. VOLUME_DOWN is OK.]\nLock state spoof: %s\n\n";
    video_set_cursor(video_get_rows()/2,0);
    video_printf(title_msg, status);
    
    for (int i = 0;i < 6;i++) {
        switch (i) {
            case 0:
                video_printf("[Recovery    Mode]");
                break;
            case 1:
                video_printf("[Fastboot    Mode]");
                break;
            case 2:
                video_printf("[FastbootD   Mode]");
                break;
            case 3:
                video_printf("[Normal      Mode]");
                break;
            case 4:
                video_printf("[Shutdown        ]");
                break;
            case 5:
                video_printf("[Spoofing  Switch]");
            default:
        }
        if (i == index) {
            video_printf("     <<==\n");
        } else {
            video_printf("         \n");
        }
    }
}

unsigned int boot_menu_select(void) {
    mtk_wdt_disable();
    video_clean_screen();
    mt_show_logo();
    update_menu(0);
    video_set_cursor(video_get_rows()-2, 0);
    show_bootmode(get_bootmode());
    video_set_cursor(video_get_rows()/2,0);
    unsigned int select = 0;
    while (1) {
        if (mtk_detect_key(VOLUME_UP)) {
            select = (select + 1) % 6;
            update_menu(select);
            mdelay(300);
        } else if (mtk_detect_key(VOLUME_DOWN)) {
            break;
        }
        mdelay(10);
    }
    video_clean_screen();
    video_set_cursor(video_get_rows()/2,0);
    return select;
}

static int show_boot_menu(void* arg) {
    bool exit = false;
    while (!exit) {
        mdelay(100);
        unsigned int select = boot_menu_select();
        switch (select) {
            case 0:
                cmd_reboot_recovery();
                exit = true;
                break;
            case 1:
                cmd_reboot_bootloader();
                exit = true;
                break;
            case 2:
                cmd_reboot_fastboot();
                exit = true;
                break;
            case 3:
                cmd_reboot();
                exit = true;
                break;
            case 4:
                mt_power_off();
                exit = true;
                break;
            case 5:
                switch_spoof_status();
                break;
            default:
        }
    }
    return 0;
}

static void patch_cmdline(char* cmdline) {
    cmdline_replace(cmdline, "androidboot.verifiedbootstate=", "green", "orange");
    cmdline_replace(cmdline, "androidboot.secureboot=", "1", "0");
    cmdline_replace(cmdline, "androidboot.vbmeta.device_state=", "locked", "unlocked");
}

static void handle_recovery_boot(void) {
    if (get_bootmode() != BOOTMODE_RECOVERY || !is_spoofing_enabled()) return;
    printf("Recovery boot detected, modifying cmdline for unlocked state.\n");
    static const uint32_t cmdline_addrs[] = {CMDLINE1_ADDR, CMDLINE2_ADDR};
    for (int i = 0; i < ARRAY_SIZE(cmdline_addrs); i++) {
        printf("Patching cmdline at 0x%08X\n", cmdline_addrs[i]);
        patch_cmdline((char*)cmdline_addrs[i]);
    }
}

void spoof_lock_state(void) {
    uint32_t addr = 0;

    // Branching to the required codes in order to skip security checks for fastboot commands
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4880, 0xB087, 0x4D5A);
    if (addr) {
        printf("Found fastboot_handler at 0x%08X\n", addr);
        PATCH_MEM(addr + 0xEC, 0xE008);
        NOP(addr + 0x122, 1);
        NOP(addr + 0x12C, 1);
    }

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
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF052, 0xBE10, 0xF052, 0xBE3E);
    if (addr) {
        printf("Found seccfg_get_lock_state thunk at 0x%08X\n", addr);
        PATCH_BRANCH(addr, (void*)get_lock_state);
    }

    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");
    if (!spoofing) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    // Since we're spoofing the LKS_STATE as locked, get_vfy_policy would normally
    // require partition verification during boot. Force it to return 0 to disable
    // this verification and allow booting with modified partitions.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF67, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Since we're spoofing the LKS_STATE as locked, get_dl_policy would normally
    // restrict fastboot downloads/flashing based on security policy. Force it to
    // return 0 to bypass these restrictions and allow unrestricted flashing.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF61, 0xF000);
    if (addr) {
        printf("Found dl_policy at 0x%08X\n", addr);
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
        NOP(addr + 0x98, 6);
    }

    // is set to "orange" so fastbootd detects the device as unlocked
    // and allows flashing. We also patch a few other cmdline params
    // (secureboot, device_state) as a precaution in case stock
    // recovery checks them as well.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF014, 0xFA18, 0xF001, 0xFBA4);
    if (addr) {
        printf("Found cmdline_pre_process at 0x%08X\n", addr);
        PATCH_CALL(addr, (void*)handle_recovery_boot, TARGET_THUMB);
    }

    // AVB verifies vbmeta public keys in two places: once for the main
    // vbmeta image (validate_vbmeta_public_key) and once for chained
    // vbmeta images (avb_safe_memcmp against the expected key). Both
    // reject the boot if the key doesn't match, causing the "Public key
    // used to sign data rejected" error. We patch both checks so any
    // key is accepted regardless.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xF2AD, 0x5D44);
    if (addr) {
        printf("Found load_and_verify_vbmeta at 0x%08X\n", addr);
        PATCH_MEM(addr + 0x390, 0x451B);
        NOP(addr + 0x6BC, 2);
        PATCH_MEM(addr + 0x72E, 0x2301);
    }
}

void board_early_init(void) {
    uint32_t addr = 0;
    printf("Entering early init for Realme 8i\n");

    // BBK added a verification check to ensure the device was officially unlocked.
    // If the check fails, the bootloader exits fastboot mode and reboots.
    //
    // This is unnecessary, seccfg-based unlocks are already valid, so we patch
    // the check to always return true, ensuring fastboot remains accessible.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7C9, 0xF80F, 0xF7C9);
    if (addr) {
        printf("Found unlock_verify_ok at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // The environment area is not yet initialized when board_early_init runs,
    // so environment variable checks will always return NULL at this stage.
    // To work around this timing issue, we hook into a printf call that executes
    // after environment initialization is complete and redirect it to our
    // spoof_lock_state function.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF03B, 0xFC62, 0x6823, 0x2000);
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void*)spoof_lock_state, TARGET_THUMB);
    }

    // Register our custom fastboot commands.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 0);
    fastboot_register("oem shutdown", cmd_shutdown, 0);

    // On a locked bootloader, attempting to boot into fastboot forces the
    // device to switch to recovery mode. This patch fixes this behavior and also
    // enables booting into fastboot mode by pressing Volume Up.
    PATCH_MEM(0x4C403ACC, 0x2363);
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }
}

void board_late_init(void) {
    printf("Entering late init for Realme 8i\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    //
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0A, 0x447B, 0x681B, 0x681B, 0x2B02);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200, 0x4604);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Show menu only when volume up pressed
    if (get_bootmode() == BOOTMODE_FASTBOOT && mtk_detect_key(VOLUME_UP)) {
        thread_t* thr;
        thr = thread_create("show_boot_menu", show_boot_menu, NULL, LOW_PRIORITY, DEFAULT_STACK_SIZE);
        if (thr) thread_resume(thr);
    }

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_RECOVERY && get_bootmode() != BOOTMODE_NORMAL) {
        show_bootmode(get_bootmode());
    }
}
