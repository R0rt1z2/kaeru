//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include "include/mt8516-common.h"

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

static inline void lp5562_write(uint8_t reg, uint8_t val) {
    ((void (*)(uint8_t, uint8_t))(LP5562_WRITE_FUNC_ADDR|1))(reg, val);
}

static void led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    lp5562_write(LP5562_W_PWM, 0);
    lp5562_write(LP5562_LED_MAP, 0x1b);

    // Yes, BGR is on purpose.
    lp5562_write(LP5562_R_PWM, b);
    lp5562_write(LP5562_G_PWM, g);
    lp5562_write(LP5562_B_PWM, r);
}

static void led_off(void) {
    lp5562_write(LP5562_R_PWM, 0);
    lp5562_write(LP5562_G_PWM, 0);
    lp5562_write(LP5562_B_PWM, 0);
    lp5562_write(LP5562_W_PWM, 0);
    lp5562_write(LP5562_ENABLE, 0x00);
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

void device_early_init(void) {
    // Do not set the LED color to green in fastboot mode.
    FORCE_RETURN(FB_LED_GREEN_FUNC_CALLER_ADDR, 0);
}

void device_fastboot_init(void) {
    // Kick off our LED animation.
    thread_t *thr = thread_create("led_thread", led_thread, NULL,
                                  LOW_PRIORITY, DEFAULT_STACK_SIZE);
    if (thr) thread_resume(thr);
}

void device_fastboot_cmd_reboot(const char *arg, void *data, unsigned sz) {
    // Turn off the LED, we don't want it to stay on after reboot.
    led_off();
}
