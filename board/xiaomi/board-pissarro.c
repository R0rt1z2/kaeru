//
// SPDX-FileCopyrightText: 2026 z3rokwq <z3rokwq@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

// Under stage1 the payload runs from malloc'd heap, far outside the ~±4MB that
// PATCH_CALL/PATCH_BRANCH can encode. So every LK -> payload redirect hops
// through an absolute veneer laid in LK text. It's a tail branch, so 
// lr and r0-r3 pass through untouched; ip is call-clobbered so trashing it is safe.
#define VENEER_SIZE 12u

static void install_veneer(uint32_t slot, void* target) {
    WRITE32(slot + 0, 0xC004F8DFu);           // "ldr.w ip, [pc, #4]"
    WRITE16(slot + 4, 0x4760u);               // "bx ip"
    WRITE16(slot + 6, 0xBF00u);               // "nop"
    WRITE32(slot + 8, (uint32_t)target | 1u); // ".word target|1"
}

// Original fdt_bootargs setter (FUN_4822FCA8): commits the assembled cmdline
// into the device tree's /chosen/bootargs. Called from LK's boot path via the
// BL at 0x4822B234 (DECODE_BL_TARGET(0x4822B234) == 0x4822FCA8). We wrap that
// BL, so the original bytes are gone; the wrapper chains back here by absolute
// pointer (range-independent, unlike a relative BL).
#define FDT_BOOTARGS_SETTER_ADDRESS 0x4822FCA8u

// Wrapper installed over the fdt_bootargs setter call. Right before LK commits
// the assembled cmdline into /chosen/bootargs, run handle_recovery_boot()
// so it can flip verifiedbootstate "green" -> "orange" in the cmdline buffer,
// then chain to the original setter so patched cmdline is what kernel receives.
// Both inner calls are range-independent, so only LK -> wrapper edge needs veneer.
static void recovery_cmdline_hook(uint32_t fdt) {
    handle_recovery_boot();
    ((void (*)(uint32_t))(FDT_BOOTARGS_SETTER_ADDRESS | 1))(fdt);
}

static void spoof_lock_state(void) {
    uint32_t addr = 0;

    // Xiaomi doesn't rely on MediaTek's seccfg lock state alone. Their getter
    // reads the seccfg lock state *and* a second (RPMB-backed) source and only
    // agrees when both match, otherwise it forces "locked".
    //
    // Every lock-state consumer in LK funnels through this single getter, so
    // instead of chasing individual call sites we redirect its entry to our own
    // get_lock_state() (same ABI: writes the state through the pointer in r0,
    // returns 0). Reports the real unlocked state normally and a spoofed
    // "locked" state when the user enables spoofing.
    uint32_t getter = SEARCH_PATTERN(LK_START, LK_END, 0xB5F0, 0xB087, 0x4607, 0xF243);
    if (getter) {
        printf("Found sec_get_lock_state at 0x%08X\n", getter);
        install_veneer(getter, (void*)get_lock_state);
    }

    // Once the getter reports "locked" (either really, or spoofed), the fastboot
    // command dispatcher starts refusing commands with "not support on security"
    // or "not allowed in locked state". The dispatcher matches a command, then
    // gates the actual handler call behind two conditional branches; when the
    // gate fails it falls through to the failure path instead of invoking the
    // handler.
    //
    // We neutralize both gate branches so every matched command reaches its
    // handler (the blx that follows), regardless of the reported lock state.
    // This keeps full fastboot access available even while spoofing "locked".
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB087, 0x595D, 0x4863, 0x4964);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);

        // NOP the "cbnz r0 -> c3ba" to skip handler when the name/permission strcmp fails
        NOP(addr + 0xFE, 1);

        // NOP the "cbz r0 -> c3ba" to skip handler when the unlock check reports "locked"
        NOP(addr + 0x108, 1);
    }

    // AVB rejects boot images signed with a key it doesn't trust, both for the
    // main vbmeta image and for chained vbmeta partitions, producing the
    // "Public key used to sign data rejected" error. Patch both checks so any
    // signing key is accepted, which is required to boot custom/modified images.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF47F, 0xAE71, 0xE68D, 0xF8DD);
    if (addr) {
        printf("Found load_and_verify_vbmeta at 0x%08X\n", addr);

        // Replace "cmp r2, r3" into "cmp r3, r3" so the length check always passes
        // and execution reaches the memcmp we NOP below.
        PATCH_MEM(addr - 0x320, 0x451B);

        // NOP the "bne.w" that rejects a mismatched chained vbmeta key, falling
        // through to the success path unconditionally.
        NOP(addr, 2);

        // Replace "cmp r3, #0" with "movs r3, #1" so key_is_trusted is always
        // nonzero and the following branch takes the success path.
        PATCH_MEM(addr + 0x70, 0x2301);
    }

    // The cmdline is assembled into a fixed buffer and committed into /chosen bootargs
    // by fdt_bootargs() (FUN_4822FCA8). We wrap the BL to it so handle_recovery_boot()
    // flips verifiedbootstate green->orange in the buffer just before it is read into
    // the DTB, but only when booting recovery with spoofing enabled. The wrapper 
    // (recovery_cmdline_hook) chains to the original setter afterwards. And since
    // wrapper lives in our payload, so the BL hops through a veneer parked in the now-dead
    // getter body, which is safe because the getter's entry was already redirected above.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF004, 0xFD38, 0x6833, 0x2B00);
    if (addr && getter) {
        printf("Found fdt_bootargs setter call at 0x%08X\n", addr);
        install_veneer(getter + VENEER_SIZE, (void*)recovery_cmdline_hook);
        PATCH_CALL(addr, (void*)(getter + VENEER_SIZE), TARGET_THUMB);
    }

    // Expose the current spoofing state so it can be queried over fastboot.
    fastboot_publish("kaeru-bldr-spoofing", is_spoofing_enabled() ? "1" : "0");
}

void board_early_init(void) {
    printf("Entering early init for Redmi Note 11 Pro+ 5G\n");

    uint32_t addr = 0;

    // Force get_vfy_policy to return 0 so certificate verification is skipped
    // for every partition and firmware image (boot, recovery, dtbo, SCP, ...),
    // letting the device boot modified or unsigned images.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF5F, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Same idea for the download policy: forcing get_dl_policy to return 0 marks
    // no partition as download-forbidden, so fastboot flashing works everywhere.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF59, 0xF000);
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // This handles certificate chain/hash verification for the modem images
    // (md1rom, md3rom, ...). Force it to return 0 so modem images load without
    // passing signature verification.
    uint32_t ccci = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0x460A, 0x4604);
    if (ccci) {
        printf("Found ccci_ld_md_sec_ptr_hdr_verify at 0x%08X\n", ccci);
        FORCE_RETURN(ccci, 0);
    }

    // The environment isn't initialized when board_early_init runs, so get_env
    // would return NULL and our spoof helpers can't read their settings yet.
    // platform_init prints a profiling message right after ENV init completes;
    // that BL is non-essential, so we hijack it to run spoof_lock_state() once
    // the environment is ready, because under stage1 spoof_lock_state lives in heap,
    // out of BL range, so the env hook targets a veneer laid in the dead 
    // ccci body (ccci + 4) that does the absolute jump.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF03C, 0xFE04, 0x6823, 0x2000);
    if (addr && ccci) {
        printf("Found env_init_done at 0x%08X\n", addr);
        install_veneer(ccci + 4, (void*)spoof_lock_state);
        PATCH_CALL(addr, (void*)(ccci + 4), TARGET_THUMB);
    }

    // Register our custom fastboot command to toggle lock-state spoofing.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 0);
}

void board_late_init(void) {
    printf("Entering late init for Redmi Note 11 Pro+ 5G\n");

    uint32_t addr = 0;

    // Disable the dm-verity corruption warning shown at boot on unlocked
    // devices. Without this, the user gets a scary "Your device is corrupt"
    // screen that waits for a key press and powers off after 5 seconds.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}
