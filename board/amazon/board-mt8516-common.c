//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/bcb_amzn/bcblib.h>

#include "include/mt8516-common.h"

static struct {
    bool unlocked_critical;
    uint64_t misc_offset;
    struct bcb bcb;
    bool bcb_dirty;
    bool have_booted_slot;
    int booted_slot;
} gd;

static bool get_misc_offset(uint64_t *offset) {
    if (gd.misc_offset) {
        *offset = gd.misc_offset;
        return true;
    }

    part_t* misc_part = mt_part_get_partition("misc");
    if (!misc_part) {
        printf("misc partition not found\n");
        return false;
    }

    gd.misc_offset = misc_part->start_sect * BLOCK_SIZE;
    *offset = gd.misc_offset;
    return true;
}

static void print_bcb(void)
{
    int active_slot = bcblib_bcb_get_active_slot(&gd.bcb, false, false);
    printf("BCB [Magic: 0x%0x | Ver: %d]\n", gd.bcb.magic, gd.bcb.version);
    printf("Slot A: prio=%-2d tries=%-1d success=%d\n",
           gd.bcb.slot[0].priority, gd.bcb.slot[0].tries, gd.bcb.slot[0].success);
    printf("Slot B: prio=%-2d tries=%-1d success=%d\n",
           gd.bcb.slot[1].priority, gd.bcb.slot[1].tries, gd.bcb.slot[1].success);
    if (active_slot >= 0)
        printf("Active slot: %c\n", 'a' + active_slot);
    else
        printf("Active slot: NONE; WILL FAIL TO BOOT!\n");
}

static bool commit_bcb(void) {
    if (!gd.bcb_dirty) {
        printf("%s: Current BCB is clean, not writing\n", __func__);
        return true;
    }

    struct device_t *dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("%s: Block device not initialized for misc writing\n", __func__);
        return false;
    }

    size_t w = dev->write(dev, &gd.bcb, gd.misc_offset + BCB_OFFSET, sizeof(struct bcb), USER_PART);
    if (w != sizeof(struct bcb)) {
        printf("%s: Failed to commit BCB\n", __func__);
        return false;
    }

    gd.bcb_dirty = false;
    return true;
}

static bool load_bcb(void) {
    // If the current BCB is dirty, then the course of action is
    // to re-load the fresh state from storage. Otherwise just
    // make sure the current BCB in memory isn't broken.
    if (!gd.bcb_dirty) {
        printf("%s: BCB is already loaded\n", __func__);
        goto check_bcb;
    }

    struct device_t *dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("%s: Block device not initialized for misc reading\n", __func__);
        return false;
    }

    // Read in the BCB
    uint64_t misc_offset;
    if (!get_misc_offset(&misc_offset)) {
        return false;
    }

    // BCB lives at 0x360 into the misc partition.
    misc_offset += BCB_OFFSET;
    size_t read = dev->read(dev, misc_offset, &gd.bcb, sizeof(struct bcb), USER_PART);
    if (read != sizeof(struct bcb)) {
        printf("%s: Failed to read BCB\n", __func__);
        return false;
    }

    gd.bcb_dirty = false;

check_bcb:
    // Sanity check BCB
    printf("%s: Sanity check BCB...\n", __func__);

    if (!bcblib_bcb_magic_valid(&gd.bcb)) {
        printf("%s: BCB magic is invalid! Re-initialising.\n", __func__);
        bcblib_bcb_init(&gd.bcb);

        // Make sure we mark a slot as successful, the defaults will
        // eventually brick the device.
        gd.bcb.slot[0] = BCB_SLOT_METADATA_ACTIVE;
        gd.bcb.slot[1] = BCB_SLOT_METADATA_EMPTY;

        gd.bcb_dirty = true;
    }
    else if (!bcblib_metadata_get_success(&gd.bcb.slot[0]) &&
            !bcblib_metadata_get_success(&gd.bcb.slot[1])) {
        printf("%s: Both slots are failed, fixing.\n", __func__);

        // If both slots are marked as failed, set success on the
        // highest priority slot (otherwise we brick).
        int current = bcblib_bcb_get_active_slot(&gd.bcb, false, false);
        if (current < 0)
            current = 0;

        bcblib_metadata_set_success(&gd.bcb.slot[current], true);

        gd.bcb_dirty = true;
    } else {
        printf("%s: BCB is okay\n", __func__);
    }

    return commit_bcb();
}

static bool advance_partition_name(const char** partition) {
    if (!partition || !*partition) {
        return false;
    }

    while (**partition != '\0' && ISSPACE(**partition)) {
        (*partition)++;
    }

    return (**partition != '\0');
}

static bool is_partition_protected(const char* partition, bool erase) {
    // These partitions are critical, flashing them incorrectly can lead to a hard brick.
    // To prevent accidental damage, we mark them as protected and block write access.
    if (strcmp(partition, "lk") == 0 || strcmp(partition, "lk_a") == 0 ||
        strcmp(partition, "lk_b") == 0 || strcmp(partition, "tee1") == 0 ||
        strcmp(partition, "preloader") == 0 || strcmp(partition, "misc") == 0) {
        return !gd.unlocked_critical;
    }

    // Erasing the partition where kaeru would be installed is also a bad idea...
    if (erase && strcmp(partition, CONFIG_BOOTLOADER_PARTITION_NAME) == 0) {
        return !gd.unlocked_critical;
    }

    return 0;
}

static void critical_op_fail(const char *msg) {
    fastboot_info("");
    fastboot_info(msg);
    fastboot_info("This may BRICK your device with NO WAY TO RECOVER!");
    fastboot_info("You may allow this if you know what you're doing with:");
    fastboot_info("'fastboot flashing unlock_critical'");
    fastboot_info("You will be on your own from then on.");
    fastboot_fail("Partition is protected");
}

static void cmd_flash_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = arg;
    advance_partition_name(&part);

    if (strcmp(part, "lk") == 0 || strcmp(part, "lk_a") == 0 ||
        strcmp(part, "lk_b") == 0) {
        // Redirect to where LK is actually loaded from.
        arg = CONFIG_BOOTLOADER_PARTITION_NAME;
    } else if (strcmp(part, "recovery") == 0 || strcmp(part, "recovery_a") == 0 ||
               strcmp(part, "recovery_b") == 0) {
        // Redirect to the actual recovery partition.
        arg = RECOVERY_PARTITION;
    } else if (is_partition_protected(part, false)) {
        critical_op_fail("You are attempting to flash to a critical partition.");
        return;
    }

    cmd_flash(arg, data, sz);
}

static void cmd_erase_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = arg;
    advance_partition_name(&part);

    if (is_partition_protected(part, true)) {
        critical_op_fail("You are attempting to erase a critical partition.");
        return;
    }

    cmd_erase(arg, data, sz);
}

static void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz) {
#ifdef HAVE_FASTBOOT_CMD_REBOOT
    device_fastboot_cmd_reboot(arg, data, sz);
#endif

    if (arg && strcmp(arg, "-recovery") == 0) {
        cmd_oem_reboot_recovery("", data, sz);
        return;
    }

    cmd_reboot("", data, sz);
}

static void cmd_unlock_critical(const char *arg, void *data, unsigned sz) {
    gd.unlocked_critical = true;
    fastboot_okay("");
}

static void cmd_lock_critical(const char *arg, void *data, unsigned sz) {
    gd.unlocked_critical = false;
    fastboot_okay("");
}

static void cmd_set_active(const char *arg, void *data, unsigned sz) {
    const char *slot = arg;

    if (*slot != 'a' && *slot != 'b') {
        fastboot_fail("Invalid slot. Use 'a' or 'b'");
        return;
    }

    if (!load_bcb()) {
        fastboot_fail("Failed to load BCB");
        return;
    }

    int idx = *slot - 'a';
    gd.bcb.slot[idx] = BCB_SLOT_METADATA_ACTIVE;
    gd.bcb.slot[1 - idx] = BCB_SLOT_METADATA_EMPTY;
    gd.bcb_dirty = true;

    if (!commit_bcb()) {
        fastboot_fail("Failed to write BCB");
        return;
    }

    fastboot_okay("");
}

static void cmd_print_bcb(const char *arg, void *data, unsigned sz) {
    char buf[128];

    if (!load_bcb()) {
        fastboot_fail("Failed to load BCB");
        return;
    }

    int active_slot = bcblib_bcb_get_active_slot(&gd.bcb, false, false);

    npf_snprintf(buf, sizeof(buf), "BCB [Magic: 0x%0x | Ver: %d]", gd.bcb.magic, gd.bcb.version);
    fastboot_info(buf);

    npf_snprintf(buf, sizeof(buf), "Slot A: prio=%-2d tries=%-1d success=%d",
             gd.bcb.slot[0].priority, gd.bcb.slot[0].tries, gd.bcb.slot[0].success);
    fastboot_info(buf);

    npf_snprintf(buf, sizeof(buf), "Slot B: prio=%-2d tries=%-1d success=%d",
             gd.bcb.slot[1].priority, gd.bcb.slot[1].tries, gd.bcb.slot[1].success);
    fastboot_info(buf);

    if (active_slot >= 0)
        npf_snprintf(buf, sizeof(buf), "Active slot: %c", 'a' + active_slot);
    else
        npf_snprintf(buf, sizeof(buf), "Active slot: NONE; WILL FAIL TO BOOT!");
    fastboot_info(buf);

    fastboot_okay("");
}

static void cmd_reload_bcb(const char *arg, void *data, unsigned sz) {
    gd.bcb_dirty = true;
    if (!load_bcb()) {
        fastboot_fail("Failed to re-load BCB");
        return;
    }

    fastboot_okay("");
}

static uint32_t get_active_slot(void) {
    // LK will not invoke this function if booted directly
    // to fastboot mode.
    if (gd.have_booted_slot)
        return gd.booted_slot;

    if (!load_bcb()) {
        printf("Failed to load BCB\n");
        return (uint32_t)-1;
    }

    print_bcb();

    // Choose the slot we will boot from.
    gd.booted_slot = bcblib_bcb_get_active_slot(&gd.bcb, false, false);

    // load_bcb() should fix-up the BCB if needed, but well...
    if (gd.booted_slot >= 0)
        gd.have_booted_slot = true;

    return gd.booted_slot;
}

static void fastboot_init_hook(const char *) {
    int active_slot = get_active_slot();

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);

    // Support slot switching from fastboot.
    fastboot_register("set_active:", cmd_set_active, 1);
    fastboot_register("oem print-bcb", cmd_print_bcb, 1);
    fastboot_register("oem reload-bcb", cmd_reload_bcb, 1);
    fastboot_publish("slot-count", "2");
    if (active_slot >= 0 && active_slot < 2)
        fastboot_publish("current-slot", active_slot ? "b" : "a");

#ifdef HAVE_FASTBOOT_INIT
    device_fastboot_init();
#endif
}

static const char *get_boot_part_hook(void) {
    if (get_bootmode() == BOOTMODE_RECOVERY) {
        // Override the name of the boot partition to
        // the recovery one regardless of slot.
        return RECOVERY_PARTITION;
    }

    return get_boot_part();
}

void board_early_init(void) {
    printf("Entering early init for %s\n", BOARD_NAME);

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Make sure the BCB gets loaded on the first
    // call of load_bcb().
    gd.bcb_dirty = true;

    // Force the permament unlock check to return 1.
    FORCE_RETURN(UNLOCK_CHECK_FUNC_ADDR, 1);

    // Do not forcefully set SELinux state on cmdline.
    FORCE_RETURN(SELINUX_CMDLINE_FUNC_ADDR, 0);

    // Do not forcefully set dm-verity mode on cmdline.
    FORCE_RETURN(VERITY_CMDLINE_FUNC_ADDR, 0);

    // Disable built-in fastboot command(s)
    NOP(FB_FLASH_CMD_REGISTER_CALLER, 2);      // fastboot flash
    NOP(FB_ERASE_CMD_REGISTER_CALLER, 2);      // fastboot erase
    NOP(FB_REBOOT_CMD_REGISTER_CALLER, 2);     // fastboot reboot
    NOP(FB_REBOOT_BL_CMD_REGISTER_CALLER, 2);  // fastboot reboot-bootloader
    NOP(FB_SET_ACTIVE_CMD_REGISTER_CALLER, 2); // fastboot set_active

    // Replace Amazon's BCB load function to work around a nasty
    // Preloader "feature" that bricks with the default BCB values.
    PATCH_CALL(GET_ACTIVE_SLOT_FUNC_CALLER_ADDR, &get_active_slot, TARGET_THUMB);

    // Redirect the call that prints "fastboot_init()\n" to our
    // hook, so we can register custom fastboot commands and
    // other things.
    PATCH_CALL(FB_INIT_LOG_FUNC_CALLER_ADDR, &fastboot_init_hook, TARGET_THUMB);

    // Hook get_boot_part() so we can load recovery from a dedicated
    // partition instead of dealing with SAR shenanigans.
    PATCH_CALL(GET_BOOT_PART_FUNC_CALLER_ADDR, &get_boot_part_hook, TARGET_THUMB);

#ifdef HAVE_EARLY_INIT
    device_early_init();
#endif
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

    // Disable dm-verity, we won't be needing it anymore :)
    cmdline_append("androidboot.veritymode=disabled");

#ifdef HAVE_LATE_INIT
    device_late_init();
#endif
}
