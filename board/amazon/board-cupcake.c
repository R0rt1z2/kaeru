//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

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

static bool unlocked_critical = false;

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
        strcmp(partition, "preloader") == 0) {
        return !unlocked_critical;
    }

    // Erasing the partition where kaeru would be installed is also a bad idea...
    if (erase && strcmp(partition, CONFIG_BOOTLOADER_PARTITION_NAME) == 0) {
        return !unlocked_critical;
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
    unlocked_critical = true;
    fastboot_okay("");
}

static void cmd_lock_critical(const char *arg, void *data, unsigned sz) {
    unlocked_critical = false;
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

static void fastboot_init_hook(const char *) {
    thread_t* thr;

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);

    // Kick off our LED animation.
    thr = thread_create("led_thread", led_thread, NULL, LOW_PRIORITY, DEFAULT_STACK_SIZE);
    if (thr) thread_resume(thr);

    return;
}

void board_early_init(void) {
    printf("Entering early init for Echo Input\n");

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
