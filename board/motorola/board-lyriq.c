//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void sp_init_hook(void) {
    // Motorola keeps the lock state in two magic cookies that the SP
    // partition code installs. 0x77EE is unlocked, 0x88FF is locked, and
    // 0x66CC is a second cookie the fastboot gates read on their own.
    //
    // Plant the unlocked pair so all of LK sees a real unlock.
    *(volatile uint32_t*)0x4841FC08 = 0x77EE;
    *(volatile uint32_t*)0x4841F9E8 = 0x66CC;
    FORCE_RETURN(0x4828CEC8, 1); // gvb_unlocked check
}

static int gvb_verify_hook(void) {
    int ret = ((int (*)(void))(0x4826E3D8 | 1))();

    // LK picks the warning logo from this straight after the call above,
    // and 0 is what a green boot state leaves here. No logo is drawn for
    // it, so the device stays quiet whatever the real state was.
    *(volatile uint32_t*)0x4841F9E4 = 0;

    return ret;
}

static void avb_set_boot_state(void) {
    *(volatile uint32_t*)0x4839B3BC =
            get_bootmode() == BOOTMODE_RECOVERY ? 2 : 0;
}

// kaeru bootloader lock spoofing control command.
FASTBOOT_CMD(bldr_spoof, "oem bldr_spoof", cmd_spoof_bootloader_lock, 0);

void board_early_init(void) {
    printf("Entering early init for Motorola Edge 40\n");

    // The SP partition decides the lock state during boot, so wait for
    // the environment to come up before overriding what it picked.
    PATCH_CALL(0x48203876, (void*)sp_init_hook, TARGET_THUMB);

    // Keeps the phone from refusing to boot once the SP data stops
    // matching what mot_sec expects.
    FORCE_RETURN(0x4828BDDC, 1);

    // Disable image verification so any partition can boot or flash, not
    // just boot and recovery.
    //
    // Custom logos, modem images and everything else signed would rejected
    // otherwise, and the stock bootloader would refuse to boot them.
    FORCE_RETURN(0x4821AA64, 0);

    // Every image flashed over fastboot is checked before it gets written,
    // and unsigned ones come back as "Preflash validation failed".
    //
    // Force the pre flash validation check so they all get accepted.
    FORCE_RETURN(0x48271610, 1);

    // Motorola draws the unlock warning from the gvb logo in the
    // MotoLogo partition. The old text screens are still in LK, but
    // nothing reaches them any more.
    //
    // Hook the check that works out which one to draw, so we can clear
    // the state it leaves behind and let the logo flow run untouched.
    PATCH_CALL(0x4826E50A, (void*)gvb_verify_hook, TARGET_THUMB);
}

static void spoof_lock_state(void) {
    if (!is_spoofing_enabled()) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    printf("Bootloader lock status spoofing enabled, applying patches.\n");

    // mot_sec has four lock state accessors and they do not agree. This
    // is the only one reaching the root of trust the TEE gets, so the
    // fastboot gates and libavb carry on seeing an unlocked device.
    FORCE_RETURN(0x4828B728, 0);

    // vbmeta.device_state comes from a libavb callback of its own that
    // never looks at the accessor above. Drop the branch in
    // avb_append_options() reaching for "unlocked".
    NOP(0x482A2FC6, 1);

    // androidboot.bl_state is built straight off the 0x77EE cookie, so
    // Android would still read 2 for unlocked. Turn the 0x88FF test into
    // cmp r3, r3 to always pick the locked value of 3.
    PATCH_MEM(0x4828BC5A, 0x429B);

    // set_boot_state() only ever reached orange through the accessor we
    // just stubbed, which leaves red for every unsigned image. Take the
    // function over and pick verifiedbootstate ourselves.
    PATCH_BRANCH(0x482A9190, (void*)avb_set_boot_state);
}

void board_late_init(void) {
    printf("Entering late init for Motorola Edge 40\n");

    spoof_lock_state();
}
