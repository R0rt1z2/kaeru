//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>
#include <lib/bcb_amzn/bcblib.h>

#define LP5562_ENABLE   0x00
#define LP5562_OP_MODE  0x01
#define LP5562_B_PWM    0x02
#define LP5562_G_PWM    0x03
#define LP5562_R_PWM    0x04
#define LP5562_CONFIG   0x08
#define LP5562_W_PWM    0x0E
#define LP5562_LED_MAP  0x70

#define LED_ACTION_DELAY 0
#define LED_ACTION_SET_R 1
#define LED_ACTION_SET_G 2
#define LED_ACTION_SET_B 3
#define FRAME_DELAY 1000

struct led_frame {
    uint8_t action;
    uint16_t value;
};

static const struct led_frame anim_frames[] = {
    { LED_ACTION_SET_R, 255 },
    { LED_ACTION_SET_G, 0   },
    { LED_ACTION_SET_B, 0   },
    { LED_ACTION_DELAY, FRAME_DELAY },

    { LED_ACTION_SET_G, 127 },
    { LED_ACTION_DELAY, FRAME_DELAY },

    { LED_ACTION_SET_G, 255 },
    { LED_ACTION_DELAY, FRAME_DELAY },

    { LED_ACTION_SET_R, 0   },
    { LED_ACTION_DELAY, FRAME_DELAY },

    { LED_ACTION_SET_G, 0   },
    { LED_ACTION_SET_B, 255 },
    { LED_ACTION_DELAY, FRAME_DELAY },

    { LED_ACTION_SET_R, 75  },
    { LED_ACTION_SET_B, 130 },
    { LED_ACTION_DELAY, FRAME_DELAY },

    { LED_ACTION_SET_R, 148 },
    { LED_ACTION_SET_B, 211 },
    { LED_ACTION_DELAY, FRAME_DELAY },
};

static struct {
    bool unlocked_critical;
    uint64_t misc_offset;
    struct bcb bcb;
    bool bcb_dirty;
    bool have_booted_slot;
    int booted_slot;
} gd;

static void cmd_flash(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x41E1E3E1|1))(arg, data, sz);
}

static void cmd_erase(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x41E1E529|1))(arg, data, sz);
}

static void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x41E1D555|1))(arg, data, sz);
}

static void cmd_oem_reboot_recovery(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x41E1D431|1))(arg, data, sz);
}

static void cmdline_append(const char *arg) {
    ((void (*)(const char *))(0x41E1AAE0|1))(arg);
}

static void lp5562_write(uint8_t reg, uint8_t val) {
    ((void (*)(uint8_t, uint8_t))(0x41E1A91C|1))(reg, val);
}

static void mdelay(unsigned long msecs) {
    ((void (*)(unsigned long))(0x41E11650|1))(msecs);
}

static struct device_t* mt_part_get_device(void) {
    return ((struct device_t* (*)(void))(CONFIG_MT_PART_GET_DEVICE_ADDRESS|1))();
}

static part_t* mt_get_part(const char* name) {
    return ((part_t* (*)(const char*))(CONFIG_MT_PART_GET_PARTITION_ADDRESS|1))(name);
}

static bool get_misc_offset(uint64_t *offset) {
    if (gd.misc_offset) {
        *offset = gd.misc_offset;
        return true;
    }

    part_t* misc_part = mt_get_part("misc");
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

static void led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    lp5562_write(LP5562_W_PWM, 0);
    lp5562_write(LP5562_LED_MAP, 0x1b);

    // Yes, BGR is on purpose.
    lp5562_write(LP5562_R_PWM, b);
    lp5562_write(LP5562_G_PWM, g);
    lp5562_write(LP5562_B_PWM, r);
}

static void cmd_flash_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = arg;
    advance_partition_name(&part);

    if (strcmp(part, "lk") == 0 || strcmp(part, "lk_a") == 0 ||
        strcmp(part, "lk_b") == 0) {
            // Redirect to where LK is actually loaded from.
            arg = CONFIG_BOOTLOADER_PARTITION_NAME;
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

static int led_thread(void* arg) {
    uint8_t current_r = 0, current_g = 0, current_b = 0;

    while (true) {
        for (uint32_t i = 0; i < ARRAY_SIZE(anim_frames); i++) {
            switch (anim_frames[i].action) {
                case LED_ACTION_SET_R:
                    current_r = anim_frames[i].value;
                    led_set_rgb(current_r, current_g, current_b);
                    break;
                case LED_ACTION_SET_G:
                    current_g = anim_frames[i].value;
                    led_set_rgb(current_r, current_g, current_b);
                    break;
                case LED_ACTION_SET_B:
                    current_b = anim_frames[i].value;
                    led_set_rgb(current_r, current_g, current_b);
                    break;
                case LED_ACTION_DELAY:
                    mdelay(anim_frames[i].value);
                    break;
            }
        }
    }

    return 0;
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
    thread_t* thr;

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

    // Kick off our LED animation.
    thr = thread_create("led_thread", led_thread, NULL, LOW_PRIORITY, DEFAULT_STACK_SIZE);
    if (thr) thread_resume(thr);

    return;
}

void board_early_init(void) {
    printf("Entering early init for Echo Input\n");

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Make sure the BCB gets loaded on the first
    // call of load_bcb().
    gd.bcb_dirty = true;

    // Force the permament unlock check to return 1.
    FORCE_RETURN(0x41E0196C, 1);

    // Do not forcefully set SELinux state on cmdline.
    FORCE_RETURN(0x41E1BF2C, 0);

    // Do not forcefully set dm-verity mode on cmdline.
    FORCE_RETURN(0x41E1D2C4, 0);

    // Do not set the LED color to green in fastboot.
    FORCE_RETURN(0x41E0DF22, 0);

    // Disable built-in fastboot command(s)
    NOP(0x41E1CF46, 2); // fastboot flash
    NOP(0x41E1CF5A, 2); // fastboot erase
    NOP(0x41E1CF84, 2); // fastboot reboot
    NOP(0x41E1D042, 2); // fastboot set_active

    // Replace Amazon's BCB load function to work around a nasty
    // Preloader "feature" that bricks with the default BCB values.
    PATCH_CALL(0x41E1BFCC, &get_active_slot, TARGET_THUMB);

    // Hook into fastboot_init() since get_bootmode() is
    // not reliable here for detecting if we will enter
    // fastboot or not.
    PATCH_CALL(0x41E1CEA0, &fastboot_init_hook, TARGET_THUMB);
}

void board_late_init(void) {
    printf("Entering late init for Echo Input\n");

    // Disable dm-verity, we won't be needing it anymore :)
    cmdline_append("androidboot.veritymode=disabled");
}
