//
// SPDX-FileCopyrightText: 2026 yntion <you@example.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

FASTBOOT_CMD(bldr_spoof, "oem bldr_spoof", cmd_spoof_bootloader_lock, 0);

static void spoof_lock_state(void) {
	// LK has two security gates in the fastboot command processor that
	// reject commands with "not support on security" and "not allowed
	// in locked state" errors. When spoofing lock state, these would
	// block all fastboot operations despite the device being actually
	// unlocked underneath.
	//
	// Even without spoofing, we patch these out as a safety measure
	// since OEM-specific checks could still interfere with fastboot
	// commands in unexpected ways.

	// "not support on security" call
	NOP(0x48025596, 2);

	// "not allowed in locked state" call
	NOP(0x480255a2 + 0x17A, 2);

	// Jump directly to command handler
	PATCH_MEM(0x48025522, 0xE008); // b #0x10

	int spoofing = is_spoofing_enabled();
	fastboot_publish("is-spoofing", spoofing ? "1" : "0");

	if (!spoofing) {
		printf("Bootloader lock status spoofing disabled.\n");
		return;
	}

	printf("Bootloader lock status spoofing enabled, applying patches.\n");

	// AVB adds device state info to the kernel cmdline, but it
	// keeps showing "unlocked" even when we want it to say "locked".
	// This patch forces the cmdline to always use the "locked"
	// string instead of checking the actual device state.

	// NOP out the code that checks the actual device state,
	// forcing libavb to always use the "locked" string.
	NOP(0x48053ff2, 1);

	// On most MediaTek devices, lock state is fetched by calling
	// seccfg_get_lock_state() directly. Some vendors (e.g. Xiaomi)
	// add a wrapper that also checks a custom lock mechanism, but
	// this device does not have one.
	//
	// Unlike other LK images that route all callers through a b.w
	// thunk (which can be redirected with a single patch), this LK
	// calls seccfg_get_lock_state() directly, so we patch the
	// function body itself. The patch forces it to store 4 into the
	// output parameter and return 0, which the caller interprets as
	// the locked state.
	PATCH_MEM(0x48063c76,
			0x2300,  // movs r3, #4  (LKS_LOCK)
			0x6023,  // str r3, [r4, #0]
			0x2000,  // movs r0, #0
			0xBD10   // pop {r4, pc}
		 );

	// AVB verifies vbmeta public keys in two places: once for the main
	// vbmeta image (validate_vbmeta_public_key) and once for chained
	// vbmeta images (avb_safe_memcmp against the expected key). Both
	// reject the boot if the key doesn't match, causing the "Public key
	// used to sign data rejected" error. We patch both checks so any
	// key is accepted regardless.

	// The chain key check first compares key lengths before calling
	// memcmp. If lengths differ, it skips memcmp and falls straight
	// to the error path. Change "cmp r2, r3" to "cmp r3, r3" so the
	// length check always succeeds, allowing execution to reach the
	// memcmp path (which we NOP below).
	PATCH_MEM(0x480565b0, 0x451B);

	// NOP the bne.w that rejects mismatched chained vbmeta keys,
	// falling through to the success path unconditionally.
	NOP(0x480568b8, 2);

	// Replace "cmp r3, #0" with "movs r3, #1" so key_is_trusted
	// is always nonzero and the following bne.w takes the success
	// branch.
	PATCH_MEM(0x4805692a, 0x2301);
}

void board_early_init(void) {
	printf("Entering early init for Redmi 9C\n");

	// Regardless of whether spoofing is enabled, we always need to
	// disable image authentication. The user may just be using this
	// custom LK to unlock their device, or they may be spoofing
	// where the locked state would enforce verification.
	//
	// Forcing get_vfy_policy to return 0 skips certificate
	// verification for all partitions and firmware images (boot,
	// recovery, dtbo, SCP, etc.) so the device can boot with
	// modified or unsigned images.
	FORCE_RETURN(0x48018d98, 0);

	// Since we're spoofing the LKS_STATE as locked, get_dl_policy would normally
	// restrict fastboot downloads/flashing based on security policy. Force it to
	// return 0 to bypass these restrictions and allow unrestricted flashing.
	FORCE_RETURN(0x48018da4, 0);

	// The environment area isn't initialized yet when board_early_init
	// runs, so any get_env calls would return NULL at this stage. We
	// hook a printf call in platform_init that runs right after env
	// initialization completes, it's a convenient entry point since
	// the call itself is non-essential and we need the env to be ready
	// before applying our lock state patches.
	PATCH_CALL(0x48002a82, (void*)spoof_lock_state, TARGET_THUMB);
}

void board_late_init(void) {
	printf("Entering late init for Redmi 9C\n");
}
