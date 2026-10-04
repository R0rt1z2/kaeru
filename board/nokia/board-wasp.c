//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 0

#define RTC_PDN1 0x5B4
#define RTC_PDN1_FAST_BOOT (1 << 13)
#define RTC_PDN1_RECOVERY_MASK 0x30
#define RTC_PDN1_RECOVERY_VAL 0x10

#define WDT_REASON_RECOVERY 2
#define WDT_REASON_FASTBOOT 3

enum mode_reason {
    MODE_REASON_NONE = 0,
    MODE_REASON_PRELOADER = 1,
    MODE_REASON_MISC = 2,
    MODE_REASON_KEY = 3,
    MODE_REASON_RTC = 4,
};

static struct {
    enum mode_reason reason;
    uint32_t rtc_pdn1;
} gd;

static const char* modereason2str(enum mode_reason reason) {
    switch (reason) {
        case MODE_REASON_PRELOADER:
            return "Preloader";
        case MODE_REASON_MISC:
            return "BCB";
        case MODE_REASON_KEY:
            return "Volume Keys";
        case MODE_REASON_RTC:
            return "RTC";
        default:
            return "None";
    }
}

static int rtc_read(uint32_t reg, uint32_t* val) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x4602, 0x2000, 0x460B);
    if (addr) return ((int (*)(uint32_t, uint32_t*))(addr | 1))(reg, val);
    return -1;
}

static void rtc_mark_fast(int set) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0xB082, 0x4604, 0xF7FF, 0xFCEF);
    if (addr) ((void (*)(int))(addr | 1))(set);
}

static void rtc_mark_recovery(int set) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0xB082, 0x4604, 0xF7FF, 0xFC8D);
    if (addr) ((void (*)(int))(addr | 1))(set);
}

static void mtk_detect_pmic_just_rst(void) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x480A, 0xB508, 0x4478, 0xF02B);
    if (addr) ((void (*)(void))(addr | 1))();
}

static bool preloader_boot_mode(void) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB570, 0x4D15, 0x4B15);
    if (addr) return ((bool (*)(void))(addr | 1))();
    return false;
}

static uint32_t wdt_boot_reason(void) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B08, 0x447B, 0x7818);
    if (addr) return ((uint32_t (*)(void))(addr | 1))();
    return 0;
}

static void cmd_reboot(const char* arg, void* data, unsigned sz) {
    if (arg && arg[0] && !cmd_reboot_write_message(arg)) {
        fastboot_fail("unknown reboot target or failed to write misc");
        return;
    }

    fastboot_okay("");
    mtk_wdt_reset();
}

FASTBOOT_CMD(reboot, "reboot", cmd_reboot, 1);

static void real_boot_mode_select(void) {
    // We use this opportunity to grab RTC_PDN1 for use later,
    // as RTC driver init will clear out the recovery bits,
    // which is not what we want when we have to detect
    // RTC recovery mode.
    rtc_read(RTC_PDN1, &gd.rtc_pdn1);

    // Set bootmode to BOOTMODE_NORMAL for good measure.
    set_bootmode(BOOTMODE_NORMAL);
}

static void boot_mode_select(void) {
    // Clear out the reset flag from the PMIC. We really don't care
    // about the return, but not calling this function could mess
    // things up.
    mtk_detect_pmic_just_rst();

    // The preloader hands us its boot mode (META, factory and friends) in
    // the boot args. Act on it before anything else, like stock LK does.
    if (preloader_boot_mode()) {
        gd.reason = MODE_REASON_PRELOADER;
        return;
    }

    // Act on any boot command left in misc before anything else, so a
    // key press can still override it below.
    read_and_set_bootmode_from_message();
    if (get_bootmode() != BOOTMODE_NORMAL) gd.reason = MODE_REASON_MISC;

    // Huaqin removed the ability to enter recovery mode with the volume
    // keys, we restore that functionality here.
    if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
        gd.reason = MODE_REASON_KEY;
    } else if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
        gd.reason = MODE_REASON_KEY;
    }

    // If our bootmode is STILL normal after all that, give a chance for
    // RTC to select the boot mode, for compatibility with stock OS.
    if (get_bootmode() == BOOTMODE_NORMAL) {
        uint32_t wdt_reason = wdt_boot_reason();

        if ((gd.rtc_pdn1 & RTC_PDN1_FAST_BOOT) || wdt_reason == WDT_REASON_FASTBOOT) {
            rtc_mark_fast(0);
            set_bootmode(BOOTMODE_FASTBOOT);
            gd.reason = MODE_REASON_RTC;
        } else if ((gd.rtc_pdn1 & RTC_PDN1_RECOVERY_MASK) == RTC_PDN1_RECOVERY_VAL ||
                   wdt_reason == WDT_REASON_RECOVERY) {
            rtc_mark_recovery(0);
            set_bootmode(BOOTMODE_RECOVERY);
            gd.reason = MODE_REASON_RTC;
        }
    }
}

static void spoof_lock_state(void) {
    uint32_t addr = 0;

    // fastboot turns away every command once we report locked, stock
    // ones included.
    //
    // The matched command is dispatched further down, past the gate, so
    // we branch straight there and let it run.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B65, 0x4C66, 0xE92D, 0x4880, 0xB089);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);
        PATCH_MEM(addr + 0xFA, 0xE00F);  // b <handler dispatch>
    }

    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");

    if (!spoofing) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    printf("Bootloader lock status spoofing enabled, applying patches.\n");

    // On most MediaTek devices, lock state is fetched by calling
    // seccfg_get_lock_state() directly. Some vendors (e.g. Xiaomi)
    // add a wrapper that also checks a custom lock mechanism, but
    // this device does not have one.
    //
    // Unlike other LK images that route all callers through a b.w
    // thunk (which can be redirected with a single patch), this LK
    // calls seccfg_get_lock_state() directly, so we patch the
    // function body itself. The patch forces it to store 1 into the
    // output parameter and return 2, which the caller interprets as
    // the unlocked state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B28, 0x447B, 0x681B, 0x681B, 0x2B11, 0xD02C);
    if (addr) {
        printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
        FORCE_RETURN(addr, 1);
    }

    // AVB puts vbmeta.device_state on the cmdline from the real state, so
    // left alone it says unlocked.
    //
    // NOP the branch that picks "unlocked" and it falls through to locked.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF8DB, 0x3024, 0x4658, 0x4798, 0x2801, 0xD0F4);
    if (addr) {
        printf("Found AVB device_state cmdline at 0x%08X\n", addr);
        NOP(addr + 0x10, 1);
    }

    // verifiedbootstate comes from a boot-state value (0 green, 1 orange,
    // 2 yellow, 3 red).
    //
    // Pin its load to 0 so the cmdline always reads green.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B11, 0x447B, 0x681B, 0x681B, 0x2B03);
    if (addr) {
        printf("Found verifiedbootstate cmdline at 0x%08X\n", addr);
        PATCH_MEM(addr + 0x8, 0x2300);  // movs r3, #0
    }

    // Hook cmdline_pre_process so handle_recovery_boot() can flip
    // verifiedbootstate before LK hands the cmdline to the kernel.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF8DF, 0x0954, 0x4478, 0xF00E, 0xF984, 0xF001, 0xF94C);
    if (addr) {
        printf("Found cmdline_pre_process at 0x%08X\n", addr);
        PATCH_CALL(addr + 6, (void*)handle_recovery_boot, TARGET_THUMB);
    }
}

FASTBOOT_CMD(bldr_spoof, "oem bldr_spoof", cmd_spoof_bootloader_lock, 1);

void board_early_init(void) {
    printf("Entering early init for Nokia 2.2\n");

    uint32_t addr = 0;

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Huaqin's reboot handlers rely on RTC flags and a misc parser that
    // only knows 'boot-recovery' and 'boot-fastboot', so reboot targets
    // don't reliably land where they should.
    //
    // Drop the stock registrations so our 'reboot' above serves them
    // all of them manually.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF8DF, 0x162C, 0x2201, 0xF8DF);
    if (addr) {
        printf("Found reboot registrations at 0x%08X\n", addr);
        NOP(addr + 0x10, 2);  // reboot
        NOP(addr + 0x24, 2);  // reboot-bootloader
        NOP(addr + 0x4C, 2);  // reboot-recovery
        NOP(addr + 0x60, 2);  // reboot-fastboot
    }

    // Override LK's default boot mode handling.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x6823, 0x4681, 0x3301, 0x6023, 0xF000);
    if (addr) {
        printf("Found boot_mode_select call at 0x%08X\n", addr + 8);
        PATCH_CALL(addr + 8, (void*)real_boot_mode_select, TARGET_THUMB);
    }

    // Get rid of the stock fastboot mode string, we print our own once
    // the boot mode is settled.
    char* s = SEARCH_STRING(" => FASTBOOT mode...\n");
    if (s) {
        printf("Found fastboot string at 0x%08X\n", (uint32_t)(uintptr_t)s);
        s[0] = '\0';
    }

    // Regardless of whether spoofing is enabled, we always need to
    // disable image authentication. The user may just be using this
    // custom LK to unlock their device, or they may be spoofing
    // where the locked state would enforce verification.
    //
    // Forcing get_vfy_policy to return 0 skips certificate
    // verification for all partitions and firmware images (boot,
    // recovery, dtbo, SCP, etc.) so the device can boot with
    // modified or unsigned images.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF75, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // The environment area isn't initialized yet when board_early_init
    // runs, so any get_env calls would return NULL at this stage. We
    // hook a printf call in platform_init that runs right after env
    // initialization completes, it's a convenient entry point since
    // the call itself is non-essential and we need the env to be ready
    // before applying our lock state patches.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF02D, 0xF928, 0x6823, 0x4638, 0x3B01, 0x6023);
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void*)spoof_lock_state, TARGET_THUMB);
    }
}

void board_late_init(void) {
    printf("Entering late init for Nokia 2.2\n");

    uint32_t addr = 0;

    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && mode != BOOTMODE_POWEROFF_CHARGING && !is_unknown_mode(mode)) {
        show_bootmode(mode);
    }

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
