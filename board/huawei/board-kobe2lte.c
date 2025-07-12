//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void mtk_arch_reset(char mode) {
    ((void (*)(char))(0x48013874 | 1))(mode);
}

void reboot_emergency(void) {
    // USB download register value for bootrom mode with no timeout
    // 0x444C0000 = magic number for brom check
    // 0x0000FFFC = no timeout (max timeout value shifted)
    // 0x00000001 = download bit enabled
    uint32_t usbdl_value = 0x444C0000 | 0x0000FFFC | 0x00000001;
    
    *(volatile uint32_t*)0x1001A100 = 0xAD98;      // MISC_LOCK_KEY magic
    *(volatile uint32_t*)0x1001A108 |= 1;          // Set RST_CON bit
    *(volatile uint32_t*)0x1001A100 = 0;           // Clear MISC_LOCK_KEY
    *(volatile uint32_t*)0x1001A080 = usbdl_value; // Set BOOT_MISC0/USBDL_FLAG
    
    mtk_arch_reset(0);
}

void cmd_reboot_emergency(const char* arg, void* data, unsigned sz) {
    fastboot_info("The device will reboot into bootrom mode...");
    fastboot_okay("");
    reboot_emergency();
}

void board_early_init(void) {
    printf("Entering early init for Huawei MatePad T8 (LTE)\n");

    // Cosmetic patch to suppress the default Android logo with lock state info
    // shown during boot. This has no functional impact, purely visual.
    //
    // To restore the original behavior, simply remove this patch.
    FORCE_RETURN(0x4800F3F4, 0);
}

void board_late_init(void) {
    printf("Entering late init for Huawei MatePad T8 (LTE)\n");

    // We repurpose the ATEFACT mode to enter bootrom mode. This is useful because
    // we can control the bootmode from the Preloader VCOM interface, so we can
    // enter bootrom mode without disassembling the device even when fastboot
    // mode is unavailable.
    //
    // You can trigger this by using the handshake2.py script from amonet to send
    // b'FACTORYM' to the device.
    if (get_bootmode() == BOOTMODE_ATEFACT) {
        video_printf(" => BOOTROM mode...\n");
        reboot_emergency();
    }

    // Huawei registers a wrapper around all OEM commands, effectively blocking
    // their execution, even those explicitly registered by LK.
    //
    // This patch disables the wrapper registration entirely, allowing OEM commands
    // to run as intended without interference.
    NOP(0x48034308, 2);

    // MediaTek added a battery level check to the flash command that blocks
    // flashing when the battery is below 3%. While this might seem like a safety
    // feature, it's unnecessary, devices in fastboot are typically USB-powered.
    //
    // This patch bypasses the check entirely, allowing flashing regardless of
    // charge level.
    FORCE_RETURN(0x4803AB8C, 1);

    // This device has a struct whose pointer is returned by getter function(s). Then
    // the callers add specific offsets to the pointer to access particular fields.
    //
    // From my analysis, these offsets appear to control FBLOCK, USRLOCK, and WIDEVINE lock
    // states. Forcing these fields to 1 (unlocked) bypasses the lock state checks.
    WRITE32(0x48238008 + 0x104, 1);
    WRITE32(0x48238008 + 0x108, 1);
    WRITE32(0x48238008 + 0x10c, 1);
    WRITE32(0x48238008 + 0x114, 1);
    FORCE_RETURN(0x4802BCFC, 1);

    // Huawei defines two separate bootloader lock states: USRLOCK and FBLOCK.
    //
    // USRLOCK is cleared when the user unlocks the bootloader with an official code.
    // FBLOCK, however, remains locked and restricts flashing of critical partitions,
    // such as the bootloader itself.
    //
    // These patches force both lock state getters to always return 1 (unlocked),
    // bypassing the restrictions entirely.
    FORCE_RETURN(0x4802BAA8, 1);
    FORCE_RETURN(0x4802BA78, 1);

    // This function is used extensively throughout the fastboot implementation to
    // determine whether the device is a development unit. Until this check passes,
    // fastboot treats the device as locked, even if the bootloader is already unlocked.
    //
    // To ensure flashing works as expected, we patch this function to always return 0,
    // effectively marking the device as non-secure.
    FORCE_RETURN(0x480985C4, 1);

    // Huawei overrides standard bootmode selection with their own logic,
    // often forcing an alternative recovery mode, especially after a failed boot.
    //
    // This patch restores conventional behavior:
    // - Volume Up → Recovery
    // - Volume Down → Fastboot
    //
    // This makes bootmode selection consistent with most Android devices.
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
    } else if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x48086BE8, 0);

    // Since the only way to enter USBDL mode on this device is by opening it up
    // and shorting the test point, we add a fastboot command to make it easier
    // to enter that mode without needing to disassemble the device.
    //
    // Once you execute the command, the device will reboot into bootrom mode,
    // and you'll be able to use tools like mtkclient to flash the device.
    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        fastboot_register("oem reboot-emergency", cmd_reboot_emergency, 1);
    }

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_NORMAL && get_bootmode() != BOOTMODE_ATEFACT) {
        show_bootmode(get_bootmode());
    }
}