//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void mtk_arch_reset(char mode) {
    ((void (*)(char))(0x4800FE48 | 1))(mode);
}

long partition_write(const char* part_name, long long offset, const uint8_t* data, size_t size) {
    return ((long (*)(const char*, long long, const uint8_t*, size_t))(0x480799E8 | 1))(part_name, offset, data, size);
}

void cmd_reboot(const char* arg, void* data, unsigned sz) {
    ((void (*)(const char*, void*, unsigned))(0x48032194 | 1))(arg, data, sz);
}

void mt_power_off(void) {
    ((void (*)(void))(0x4800F96C | 1))();
}

void cmd_reboot_emergency(const char* arg, void* data, unsigned sz) {
    fastboot_info("The device will reboot into bootrom mode...");
    fastboot_okay("");
    reboot_emergency();
}

void cmd_reboot_recovery(const char* arg, void* data, unsigned sz) {
    struct misc_message misc_msg = {0};
    strncpy(misc_msg.command, "boot-recovery", 31);
    
    if (partition_write("misc", 0, (uint8_t*)&misc_msg, sizeof(misc_msg)) < 0) {
        fastboot_fail("Failed to write bootloader message!");
        return;
    }
    
    fastboot_info("The device will reboot into recovery mode...");
    fastboot_okay("");
    mtk_arch_reset(1);
}

void cmd_reboot_wrapper(const char* arg, void* data, unsigned sz) {
    // MediaTek devs couldn't figure out basic string matching, so we have to
    // do their job for them with this lovely hack. If "recovery" appears
    // anywhere in the argument, redirect to our working implementation.
    if (arg && strstr(arg, "recovery")) {
        cmd_reboot_recovery(arg, data, sz);
        return;
    }

    cmd_reboot(arg, data, sz);
    fastboot_okay("");
}

void cmd_shutdown(const char* arg, void* data, unsigned sz) {
    fastboot_info("The device will power off...");
    fastboot_info("Make sure to unplug the USB cable!");
    fastboot_okay("");
    mt_power_off();
}

void board_early_init(void) {
    printf("Entering early init for Huawei Enjoy 10e\n");

    // Cosmetic patch to suppress the default Android logo with lock state info
    // shown during boot. This has no functional impact, purely visual.
    //
    // To restore the original behavior, simply remove this patch.
    FORCE_RETURN(0x4800B9FC, 0);
}

void board_late_init(void) {
    printf("Entering late init for Huawei Enjoy 10e\n");

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
    NOP(0x48030AB4, 2);

    // MediaTek added a battery level check to the flash command that blocks
    // flashing when the battery is below 3%. While this might seem like a safety
    // feature, it's unnecessary, devices in fastboot are typically USB-powered.
    //
    // This patch bypasses the check entirely, allowing flashing regardless of
    // charge level.
    FORCE_RETURN(0x48037338, 1);

    // This device has a struct whose pointer is returned by getter function(s). Then
    // the callers add specific offsets to the pointer to access particular fields.
    //
    // From my analysis, these offsets appear to control FBLOCK, USRLOCK, and WIDEVINE lock
    // states. Forcing these fields to 1 (unlocked) bypasses the lock state checks.
    WRITE32(0x48248008 + 0x104, 1);
    WRITE32(0x48248008 + 0x108, 1);
    WRITE32(0x48248008 + 0x10c, 1);
    WRITE32(0x48248008 + 0x114, 1);
    FORCE_RETURN(0x480284C4, 1);

    // Huawei defines two separate bootloader lock states: USRLOCK and FBLOCK.
    //
    // USRLOCK is cleared when the user unlocks the bootloader with an official code.
    // FBLOCK, however, remains locked and restricts flashing of critical partitions,
    // such as the bootloader itself.
    //
    // These patches force both lock state getters to always return 1 (unlocked),
    // bypassing the restrictions entirely.
    FORCE_RETURN(0x48028270, 1);
    FORCE_RETURN(0x48028240, 1);

    // This function is used extensively throughout the fastboot implementation to
    // determine whether the device is a development unit. Until this check passes,
    // fastboot treats the device as locked, even if the bootloader is already unlocked.
    //
    // To ensure flashing works as expected, we patch this function to always return 0,
    // effectively marking the device as non-secure.
    FORCE_RETURN(0x48094D2C, 1);

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
    FORCE_RETURN(0x48083960, 0);

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        // Since the only way to enter USBDL mode on this device is by opening it up
        // and shorting the test point, we add a fastboot command to make it easier
        // to enter that mode without needing to disassemble the device.
        //
        // Once you execute the command, the device will reboot into bootrom mode,
        // and you'll be able to use tools like mtkclient to flash the device.
        fastboot_register("oem reboot-emergency", cmd_reboot_emergency, 1);

        // There is no easy way to power off the device from fastboot mode.
        // Holding the power button simply reboots the device, forcing you to
        // boot into the OS to shut it down properly. This is not ideal, so
        // we add a fastboot command that allows powering off directly.
        //
        // This was shamelessly borrowed from Motorola’s (or Huaqin’s) LK image.
        // It simply calls mt_power_off().
        fastboot_register("oem shutdown", cmd_shutdown, 1);
        fastboot_register("oem poweroff", cmd_shutdown, 1);

        // Huawei managed to screw up reboot commands in two different ways:
        //
        // 1. The command parser has a bug where "reboot-recovery" gets matched
        //    as just "reboot" (probably some dumb string comparison issue),
        //    so the device just does a normal reboot instead of going to recovery.
        //
        // 2. Even when recovery mode is triggered properly, their implementation
        //    doesn't write the bootloader message correctly to the misc partition,
        //    so recovery boot fails anyway.
        //
        // We work around both issues by disabling their broken command handlers
        // and registering our own. The reboot wrapper does a nasty string check
        // to intercept "recovery" and redirect it to our working implementation.
        NOP(0x48030BBE, 2); // register fastboot reboot recovery.
        NOP(0x48030B96, 2); // register fastboot reboot.

        // Register our non-broken reboot command handlers. For the recovery one,
        // we want to have multiple aliases: "reboot-recovery" matches the standard
        // behavior expected by fastboot, while "oem reboot-recovery" is something
        // MTK tends to add on other devices for compatibility.
        fastboot_register("reboot", cmd_reboot_wrapper, 1);
        fastboot_register("oem reboot-recovery", cmd_reboot_recovery, 1);
    }

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_NORMAL) {
        show_bootmode(get_bootmode());
    }
}