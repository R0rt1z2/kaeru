//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/bootloader_message.h>
#include <lib/bootmode.h>
#include <lib/debug.h>
#include <lib/recovery.h>
#include <lib/storage.h>
#include <lib/string.h>

static bool read_misc_partition(void *p, size_t size, uint64_t offset) {
    const char *name = CONFIG_MISC_PARTITION_NAME;
    const struct part_info *part = storage_part_find(name);
    if (!part) {
        printf("Failed to find %s partition\n", name);
        return false;
    }

    return storage_part_read(part, p, offset, size) == size;
}

static bool write_misc_partition(void *p, size_t size, uint64_t offset) {
    const char *name = CONFIG_MISC_PARTITION_NAME;
    const struct part_info *part = storage_part_find(name);
    if (!part) {
        printf("Failed to find %s partition\n", name);
        return false;
    }

    return storage_part_write(part, p, offset, size) == size;
}

bool read_bootloader_message(struct bootloader_message *boot) {
    return read_misc_partition(boot, sizeof(*boot),
                               BOOTLOADER_MESSAGE_OFFSET_IN_MISC);
}

bool write_bootloader_message(struct bootloader_message *boot) {
    return write_misc_partition(boot, sizeof(*boot),
                                BOOTLOADER_MESSAGE_OFFSET_IN_MISC);
}

bool write_bootloader_command(const char *cmd, bool overwrite) {
    struct bootloader_message boot;

    if (!read_bootloader_message(&boot))
        return false;

    if (!overwrite && boot.command[0] != '\0') {
        printf("%s: Bootloader command pending.\n", __func__);
        return false;
    }

    memset(boot.command, 0, sizeof(boot.command));
    if (cmd)
        strncpy(boot.command, cmd, sizeof(boot.command) - 1);

    return write_bootloader_message(&boot);
}

bool clear_bootloader_command(bool overwrite) {
    return write_bootloader_command(NULL, overwrite);
}

bool write_reboot_bootloader(bool overwrite) {
    return write_bootloader_command("bootonce-bootloader", overwrite);
}

bool write_reboot_recovery(bool overwrite) {
    return write_bootloader_command("boot-recovery", overwrite);
}

bool set_bootmode_from_message(struct bootloader_message *boot) {
    bootmode_t target = misc_command_to_bootmode(boot->command);

    if (target != BOOTMODE_NORMAL) {
        set_bootmode(target);

        // Consume the command here rather than trusting whatever we are
        // about to boot to do it. AOSP leaves 'boot-recovery' in place for
        // recovery to clear once it is done, but a recovery that never gets
        // around to it leaves the device looping back into recovery forever.
        //
        // Only the command goes, so the args recovery reads are untouched.
        //
        // Sticky commands are the exception, they are supposed to survive
        // and pin the device to this mode until someone erases them.
        if (misc_command_is_sticky(boot->command))
            return true;

        return clear_bootloader_command(true);
    }

    return true;
}

bool read_and_set_bootmode_from_message(void) {
    struct bootloader_message boot;
    if (!read_bootloader_message(&boot))
        return false;

    return set_bootmode_from_message(&boot);
}

bool take_boot_system_request(void) {
    struct bootloader_message boot;

    if (!read_bootloader_message(&boot))
        return false;

    if (!misc_command_is_system(boot.command))
        return false;

    return clear_bootloader_command(true);
}

bool cmd_reboot_write_message(const char *arg) {
    const char *msg = reboot_target_to_misc_command(arg);

    return msg ? write_bootloader_command(msg, true) : false;
}
