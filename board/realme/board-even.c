//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-FileCopyrightText: 2025 plushie-neko <sunflowers.warp@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

static int dprintf(const char* fmt, ...) {
    return ((int (*)(const char*))(0x4C441E28 | 1))(fmt);
}


void board_early_init(void) {
    printf("Entering early init for realme C25/S/Narzo 50A\n");

    uint32_t addr = 0;

    // BBK added a verification check to ensure the device was officially unlocked.
    // If the check fails, the bootloader exits fastboot mode and reboots.
    //
    // This is unnecessary, seccfg-based unlocks are already valid, so we patch
    // the check to always return true, ensuring fastboot remains accessible.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7C9, 0xFA0F);
    if (addr) {
        printf("Found fastboot_unlock_verify at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

#ifdef CONFIG_FORCE_LOCK_SPOOF

    // Patching the seccfg sboot-state check (FUN_4c47a9e8) to always return 1.
    // get_sboot_state (FUN_4c47aab8) stores its return value, so the device
    // reports "androidboot.sbootstate=on" regardless of the real seccfg state.
    //
    // NOTE: double_check_lock_state (RMX analog 0x4C46DAE0) is not patched yet.
    // Its strings (0x4c4c4ab8 / 0x4c4c4b00 / 0x4c4c4bb0 / 0x4c4c4c0c) have zero
    // xrefs because the body sits in an unanalyzed code gap; once located, branch
    // past the sec_set_device_lock call so first-boot doesn't force-lock the device.
    FORCE_RETURN(0x4c47a9e8, 1);
    dprintf("get_sboot_state Patched\n");

    // get_lock_state (FUN_4c47bbd8) stores (lock_state != 3) as the locked flag
    // (`subs r3,#3` then `movne r3,#1`). Overwrite the `subs r3,#3` at 0x4c47bbf0
    // with `movs r3,#1` so it always reports locked.
    PATCH_MEM(0x4c47bbf0, 0x2301);
    dprintf("get_lock_state Patched\n");

    // fastboot_init (FUN_4c42f098) publishes the getvar values from three
    // separate helpers, NOT from get_lock_state. Force the two return-value
    // helpers so the device reports locked:
    //   unlocked ← FUN_4c47d4fc = (lock_state==3) → 1 (yes) on unlocked devices; force 0 (no)
    //   secure   ← FUN_4c47d4ac = (lock_state!=3) → 0 (no) on unlocked devices; force 1 (yes)
    // FUN_4c47d4fc is also used by the fastboot dispatch gates (0x4c42edba /
    // 0x4c42ede8), but their deny branches are NOP'd above, so forcing it is safe.
    FORCE_RETURN(0x4c47d4fc, 0);
    FORCE_RETURN(0x4c47d4ac, 1);
    dprintf("fastboot getvar Patched\n");

    // androidboot.verifiedbootstate= is written into the kernel cmdline by
    // FUN_4c4558dc: it reads a global boot-state value and dispatches via tbb
    // (table @0x4c4558ee: 14 02 0e 08) to append "green" (0, 0x4c455916),
    // "yellow" (1, 0x4c4558f2), "orange" (2, 0x4c45590a) or "red" (3,
    // 0x4c4558fe). Force the dispatch index to 0 so the append always uses the
    // green case (0x4c455916).
    //   patch 0x4c4558e6  cmp r3,#0x3  ->  movs r3,#0x0  (0x2300)
    //   NOP 0x4c4558e8  bhi (bounds check, now harmless)
    PATCH_MEM(0x4c4558e6, 0x2300);
    NOP(0x4c4558e8, 1);
    dprintf("verified_boot_state Patched\n");

    // avb boot/recovery verify (FUN_4c46e37c) writes the same boot-state global
    // and would undo the green above: it inits the state to 3 (red) and, on an
    // physically unlocked device (thunk_FUN_4c47b8ac returns lock_state == 3),
    // overwrites the success value with 2 (orange). Patch both writers to force
    // green so every consumer (FUN_4c45588c / FUN_4c46e308 / cmdline / getvar)
    // reports a verified, locked boot.
    //   init  DAT_4c57b344 = 3  @ 0x4c46e3d0  movs r2,#0x3  (03 22) -> 0x2200
    //   orange override         @ 0x4c46e4fa  mov.eq r3,#0x2 (02 23) -> 0x2300
    // img_auth is already disabled by the dl/vfy policy FORCE_RETURNs above, so
    // forcing green adds no new signature requirements: avb_slot_verify's result
    // (iVar4) is zeroed and discarded when auth isn't required.
    PATCH_MEM(0x4c46e3d0, 0x2200);
    PATCH_MEM(0x4c46e4fa, 0x2300);
    dprintf("avb_boot_state_green Patched\n");

    // fastboot command dispatch (FUN_4c42ecc0) runs two per-command security
    // gates that deny execution depending on the (spoofed) lock state. NOP the
    // deny branches so every fastboot command is allowed:
    //   gate 1: deny if [r7,#0xc]==0 (0x4c42edb4), deny if lock-check returns 0 (0x4c42edbe)
    //   gate 2: same two checks (0x4c42ede2, 0x4c42edec)
    NOP(0x4c42edb4, 1);
    NOP(0x4c42edbe, 1);
    NOP(0x4c42ede2, 1);
    NOP(0x4c42edec, 1);
    dprintf("fastboot_handler Patched\n");

    // Security policy extractors over the SEC_POLICY table (FUN_4c418908):
    // FUN_4c418a3c = (val & 3) >> 1 (dl_policy) and FUN_4c418a48 = val & 1
    // (get_vfy_policy). Forcing both to 0 skips verification/auth and removes
    // flash restrictions, matching the RMX2156 port.
    FORCE_RETURN(0x4c418a3c, 0);
    FORCE_RETURN(0x4c418a48, 0);
    dprintf("policy Patched\n");

    // avb_append_options_2 (0x4c467a30) computes an "is_unlocked" flag and stores
    // it at 0x4c467a5c; the value written on entry is 0 (locked). NOP-ing the store
    // keeps avb reporting locked always.
    NOP(0x4c467a5c, 1);
    dprintf("avb_append_options Patched\n");

#endif

    // On unlocked Realme devices, volume key detection is broken, making it
    // difficult to enter fastboot or recovery mode through key combos.
    //
    // This patch restores expected behavior:
    // - Volume Up → Recovery
    // - Volume Down → Fastboot
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
    } else if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }
}

void board_late_init(void) {
    printf("Entering late init for realme C25/s/Narzo 50A\n");
 
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

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_RECOVERY
        && get_bootmode() != BOOTMODE_NORMAL) {
        show_bootmode(get_bootmode());
    }
}
