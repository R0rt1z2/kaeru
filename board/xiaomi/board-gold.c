//
// SPDX-FileCopyrightText: 2026 Ayus Chatterjee <ayuschatterjee2003@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

// Under stage1 the payload runs from malloc'd heap, far outside the ~±4MB that
// PATCH_CALL/PATCH_BRANCH can encode. So every LK -> payload redirect hops
// through an absolute veneer laid in LK text. It's a tail branch, so
// lr and r0-r3 pass through untouched; ip is call-clobbered so trashing it is safe.
#define VENEER_SIZE 12u

// On-screen bootloader glyph scale. LK's glyph renderer (FUN_48243cf4)
// takes the scale in r0; both video_printf and logo renderer hardcode a value 2
// We bump this value to 3 for a slight larger text.
#define BOOT_TEXT_FONT_SCALE 3

// Fixed-size screen printf (disp_printf). Unlike video_printf it renders with a
// hardcoded scale (no r0 scale argument), so we redirect its entry to
// video_printf (CONFIG_VIDEO_PRINTF_ADDRESS), which does use the
// scale from the two sites rewritten in board_late_init.
#define DISP_PRINTF_ADDRESS 0x482438ECu

static void install_veneer(uint32_t slot, void* target) {
    WRITE32(slot + 0, 0xC004F8DFu);           // "ldr.w ip, [pc, #4]"
    WRITE16(slot + 4, 0x4760u);               // "bx ip"
    WRITE16(slot + 6, 0xBF00u);               // "nop"
    WRITE32(slot + 8, (uint32_t)target | 1u); // ".word target|1"
}

// Original fdt_bootargs setter: commits the assembled cmdline into the device
// tree's /chosen/bootargs. We wrap its call so handle_recovery_boot() can flip
// verifiedbootstate green->orange in the buffer just before it is read into the
// DTB (only when booting recovery with spoofing enabled). The original bytes are
// gone, so the wrapper chains back here by absolute pointer (range-independent).
#define FDT_BOOTARGS_SETTER_ADDRESS 0x48233948u


// Wrap the fdt_bootargs setter call so handle_recovery_boot() rewrites
// verifiedbootstate green -> orange in the assembled cmdline right before it
// is committed to /chosen/bootargs, only when booting recovery with spoofing
// enabled. The wrapper chains back to the original setter by absolute pointer
// and hops through a veneer parked in the now-dead getter body.
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
    // get_lock_state() (same ABI). Reports the real unlocked state normally and
    // a spoofed "locked" state when the user enables spoofing.
    uint32_t getter = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0x4605, 0x4917, 0xB087);
    if (getter) {
        printf("Found get_lock_state at 0x%08X\n", getter);
        install_veneer(getter, (void*)get_lock_state);
    }

    // Fastboot command dispatcher: when the getter reports "locked", the dispatch
    // gate skips handing matched commands to their handlers. We NOP those unlock
    // checks and patch the gate so every fastboot command still runs even while
    // we spoof "locked", keeping full fastboot access available.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4C89, 0x4D8A, 0xE92D, 0x4880);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);
        NOP(addr + 0x19E, 1);
        NOP(addr + 0x1A8, 1);
        NOP(addr + 0x1D0, 1);
        NOP(addr + 0x1DA, 1);
        PATCH_MEM(addr + 0x20A, 0xE7E2);
    }

    // AVB rejects boot images signed with a key it doesn't trust, both for the
    // main vbmeta image and for chained vbmeta partitions ("Public key used to
    // sign data rejected"). Patch both checks so any signing key is accepted,
    // which is required to boot Lineage's boot/vbmeta_system/vbmeta_vendor chain.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF47F, 0xAF1C, 0xE738, 0xF8DD);
    if (addr) {
        printf("Found avb chain key check at 0x%08X\n", addr);

        // Replace "cmp r2, r3" -> "cmp r3, r3" so the length check always passes
        // and execution reaches the branch we NOP below.
        PATCH_MEM(addr - 0x1CA, 0x451B);

        // NOP the "bne.w" that rejects a mismatched chained vbmeta key, falling
        // through to the success path unconditionally.
        NOP(addr, 2);

        // Replace "cmp r3, #0" -> "movs r3, #1" so key_is_trusted is always
        // nonzero and the following branch takes the success path.
        PATCH_MEM(addr + 0x70, 0x2301);
    }

    // Wrap the fdt_bootargs setter call so recovery cmdline can be rewritten just
    // before the kernel receives it. The wrapper lives in our payload (heap), out
    // of BL range, so the redirect hops through a veneer parked in the now-dead
    // getter body (safe: the getter's entry was already redirected above).
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF004, 0xFB38, 0x4897, 0x4478);
    if (addr && getter) {
        printf("Found fdt_bootargs setter call at 0x%08X\n", addr);
        install_veneer(getter + VENEER_SIZE, (void*)recovery_cmdline_hook);
        PATCH_CALL(addr, (void*)(getter + VENEER_SIZE), TARGET_THUMB);
    }

    // Expose the current spoofing state so it can be queried over fastboot.
    fastboot_publish("kaeru-bldr-spoofing", is_spoofing_enabled() ? "1" : "0");
}

// Register kaeru bootloader lock-state spoofing control command.
FASTBOOT_CMD(bldr_spoof, "oem bldr_spoof", cmd_spoof_bootloader_lock, 0);

void board_early_init(void) {
    printf("Entering early init for Redmi Note 13 5G (gold)\n");

    uint32_t addr = 0;

    // Force "veritymode=disabled". The stock vbmeta's cmdline says "enforcing",
    // which makes Android init mount dm-verity against the stock hashtrees and
    // bootloop on a custom system. Overwrite the rodata string with "disabled".
    WRITE32(0x48330E90, 0x61736964);   
    WRITE32(0x48330E94, 0x64656C62);   
    WRITE8(0x48330E98, 0x00);          
    
    // Enlarge the AVB20 bump-allocator pool from 140MB to 384MB.
    // The LK verifies the slot TWICE (boot + recovery), each reading
    // boot(64MB) + vendor_boot(64MB) = 128MB, ~256MB total; the stock 140MB
    // pool OOM'd on the second pass. 0x18000000 = 384MB fixes it.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF04F, 0x6B0C);   
    if (addr) {
        printf("Found avb heap size (remaining) at 0x%08X\n", addr);
        PATCH_MEM(addr + 2, 0x5BC0);                        // #0x18000000 (384MB)
    }
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF04F, 0x620C);   
    if (addr) {
        printf("Found avb heap size (mblock) at 0x%08X\n", addr);
        PATCH_MEM(addr + 2, 0x52C0);                        // #0x18000000 (384MB)
    }

    // Skip the AVB data-hash digest comparison. vendor_boot is patched by
    // KernelSU, so its SHA-256 no longer matches the vbmeta descriptor
    // digest; that mismatch set "invalidate_on_error" 
    //
    // The real compare is avb_safe_memcmp @ 0x4826D758 (0=match, 1=mismatch).
    // Force it to always return 0 (match) so every digest comparison passes.
    FORCE_RETURN(0x4826D758, 0);
    printf("Forced avb_safe_memcmp (0x4826D758) to return 0\n");

    // A digest mismatch is surfaced as "return 4" (HASH_MISMATCH)
    // in FUN_4826ded8, right after the avb_safe_memcmp BL. Neutralize it too, in
    // case another code path has its own compare:
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB338, 0x4874, 0x4478);  
    if (addr) {
        printf("Found avb HASH_MISMATCH branch at 0x%08X\n", addr);
        PATCH_MEM(addr, 0xE027);                            // cbz r0 
    } 

    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2004, 0xE5E4);         
    if (addr) {
        printf("Found avb HASH_MISMATCH return at 0x%08X\n", addr); 
        PATCH_MEM(addr, 0x2000);                            // movs r0,#0
    }

    // There is an older code path returned 5 on digest mismatch
    // Neutralize it as well.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB908, 0x2005, 0xE5B5);
    if (addr) {
        printf("Found avb data hash check at 0x%08X\n", addr);
        PATCH_MEM(addr, 0xE001);                            // movs r0,#0
    }

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

    // See DISP_PRINTF_ADDRESS: make the fixed-size screen printf (boot-mode
    // banner, warning lines, fastboot status text) flow through video_printf's
    // scale-aware renderer so BOOT_TEXT_FONT_SCALE actually enlarges it.
    install_veneer(DISP_PRINTF_ADDRESS, (void*)CONFIG_VIDEO_PRINTF_ADDRESS);

    // The environment isn't initialized when board_early_init runs, so get_env
    // would return NULL and our spoof helpers can't read their settings yet.
    // env_init_done runs right after ENV init completes; we hijack it to run
    // spoof_lock_state() once the environment is ready. Under stage1
    // spoof_lock_state lives in heap, out of BL range, so the env hook targets a
    // veneer laid in the dead ccci body (ccci + 4) that does the absolute jump.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF041, 0xF95A, 0x6823, 0x2000);
    if (addr && ccci) {
        printf("Found env_init_done at 0x%08X\n", addr);
        install_veneer(ccci + 4, (void*)spoof_lock_state);
        PATCH_CALL(addr, (void*)(ccci + 4), TARGET_THUMB);
    }
}

void board_late_init(void) {
    printf("Entering late init for Redmi Note 13 5G (gold)\n");

    uint32_t addr = 0;

    // Disable the dm-verity corruption warning shown at boot on unlocked
    // devices. Without this, the user gets a scary "Your device is corrupt"
    // screen that waits for a key press and powers off after 5 seconds.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Bump the on-screen bootloader text size (see BOOT_TEXT_FONT_SCALE).
    // Two renderers pass a hardcoded glyph scale of 2 to FUN_48243cf4:
    //
    // Rewrite both "movs r0,#2" immediates (the halfword right after "mov r1,rX")
    // with BOOT_TEXT_FONT_SCALE.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4621, 0x2002, 0xF011, 0xFDB2);
    if (addr) {
        printf("Found video_printf font scale at 0x%08X\n", addr);
        PATCH_MEM(addr + 2, 0x2000 | BOOT_TEXT_FONT_SCALE);
    }
    
    // Rewrite the hardcoded glyph scale ("movs r0, #2") in the multi-line logo
    // renderer to BOOT_TEXT_FONT_SCALE so the lk texts render slightly larger.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4631, 0x2002, 0xF011, 0xFCE2);
    if (addr) {
        printf("Found multi-line text font scale at 0x%08X\n", addr);
        PATCH_MEM(addr + 2, 0x2000 | BOOT_TEXT_FONT_SCALE);
    }
}
