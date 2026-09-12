//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include "include/mt8516-common.h"

#define ISSI_REG_SHUTDOWN   0x00
#define ISSI_REG_PWM(ch)    (0x01 + (ch))
#define ISSI_REG_UPDATE     0x25

#define LED_RING_COUNT      12
#define LED_CHANNELS        (LED_RING_COUNT * 3)

#define LED_R               0
#define LED_G               1
#define LED_B               2

// The wheel is split into three linear ramps, hence 3 * 85.
#define LED_WHEEL_MAX       255

#define LED_BRIGHTNESS      0xFF
#define LED_FRAME_MS        30
#define LED_HUE_STEP        3

// Which way the rainbow travels around the ring.
#define LED_SWEEP_REVERSE   0

#define GPIO_MODE_GPIO      0
#define GPIO_DIR_IN         0
#define GPIO_PULL_UP        1

static volatile bool led_ring_animating;

static inline int mt_get_gpio_in(uint32_t pin) {
    return ((int (*)(uint32_t))(MT_GET_GPIO_IN_FUNC_ADDR|1))(pin);
}

static inline void mt_set_gpio_mode(uint32_t pin, uint32_t mode) {
    ((void (*)(uint32_t, uint32_t))(MT_SET_GPIO_MODE_FUNC_ADDR|1))(pin, mode);
}

static inline void mt_set_gpio_dir(uint32_t pin, uint32_t dir) {
    ((void (*)(uint32_t, uint32_t))(MT_SET_GPIO_DIR_FUNC_ADDR|1))(pin, dir);
}

static inline void mt_set_gpio_pull_enable(uint32_t pin, uint32_t enable) {
    ((void (*)(uint32_t, uint32_t))(MT_SET_GPIO_PULL_EN_FUNC_ADDR|1))(pin, enable);
}

static inline void mt_set_gpio_pull_select(uint32_t pin, uint32_t select) {
    ((void (*)(uint32_t, uint32_t))(MT_SET_GPIO_PULL_SEL_FUNC_ADDR|1))(pin, select);
}

static inline int issi_write(uint8_t reg, uint8_t val) {
    return ((int (*)(uint8_t, uint8_t))(ISSI_WRITE_FUNC_ADDR|1))(reg, val);
}

static inline int issi_init(void) {
    return ((int (*)(void))(ISSI_INIT_FUNC_ADDR|1))();
}

static inline void thread_sleep(unsigned long msecs) {
    ((void (*)(unsigned long))(THREAD_SLEEP_FUNC_ADDR|1))(msecs);
}

static void led_ring_write(const uint8_t *frame) {
    for (uint32_t ch = 0; ch < LED_CHANNELS; ch++)
        issi_write(ISSI_REG_PWM(ch), (frame[ch] * LED_BRIGHTNESS) / 255);

    issi_write(ISSI_REG_UPDATE, 0);
}

static void led_ring_off(void) {
    for (uint32_t ch = 0; ch < LED_CHANNELS; ch++)
        issi_write(ISSI_REG_PWM(ch), 0);

    issi_write(ISSI_REG_UPDATE, 0);
    issi_write(ISSI_REG_SHUTDOWN, 0);
}

static void hue_to_rgb(uint32_t hue, uint8_t *rgb) {
    uint8_t ramp = (hue % 85) * 3;

    if (hue < 85) {
        rgb[LED_R] = 255 - ramp;
        rgb[LED_G] = ramp;
        rgb[LED_B] = 0;
    } else if (hue < 170) {
        rgb[LED_R] = 0;
        rgb[LED_G] = 255 - ramp;
        rgb[LED_B] = ramp;
    } else {
        rgb[LED_R] = ramp;
        rgb[LED_G] = 0;
        rgb[LED_B] = 255 - ramp;
    }
}

static int led_ring_thread(void *arg) {
    uint8_t frame[LED_CHANNELS];
    uint32_t phase = 0;

    int ret = issi_init();
    if (ret) {
        printf("Failed to set up the LED ring (%d)\n", ret);
        return ret;
    }

    while (led_ring_animating) {
        for (uint32_t led = 0; led < LED_RING_COUNT; led++) {
            uint32_t hue = phase + (led * LED_WHEEL_MAX) / LED_RING_COUNT;
            hue_to_rgb(hue % LED_WHEEL_MAX, &frame[led * 3]);
        }

        led_ring_write(frame);

#if LED_SWEEP_REVERSE
        phase = (phase + LED_HUE_STEP) % LED_WHEEL_MAX;
#else
        phase = (phase + LED_WHEEL_MAX - LED_HUE_STEP) % LED_WHEEL_MAX;
#endif
        thread_sleep(LED_FRAME_MS);
    }

    return 0;
}

void device_early_init(void) {
    // LK paints the ring a solid colour every time it enters fastboot and on
    // every command it processes, which would fight our animation.
    FORCE_RETURN(ISSI_SET_STATE_FUNC_ADDR, 0);
}

static void volume_keys_init(void) {
    static const uint32_t pins[] = { GPIO_KEY_VOLUME_UP, GPIO_KEY_VOLUME_DOWN };

    for (uint32_t i = 0; i < ARRAY_SIZE(pins); i++) {
        mt_set_gpio_mode(pins[i], GPIO_MODE_GPIO);
        mt_set_gpio_dir(pins[i], GPIO_DIR_IN);
        mt_set_gpio_pull_enable(pins[i], 1);
        mt_set_gpio_pull_select(pins[i], GPIO_PULL_UP);
    }

    // Let the pull-ups drag the lines up before we sample them.
    mdelay(5);
}

static inline bool key_pressed(uint32_t pin) {
    // The buttons short to ground, so a low reads as held.
    return mt_get_gpio_in(pin) == 0;
}

void device_boot_keys(bool *up, bool *down) {
    volume_keys_init();

    *up = key_pressed(GPIO_KEY_VOLUME_UP);
    *down = key_pressed(GPIO_KEY_VOLUME_DOWN);

    printf("Volume keys: up=%d down=%d\n", *up, *down);
}

void device_fastboot_init(void) {
    led_ring_animating = true;

    thread_t *thr = thread_create("led_ring", led_ring_thread, NULL,
                                  LOW_PRIORITY, DEFAULT_STACK_SIZE);
    if (thr)
        thread_resume(thr);
    else
        led_ring_animating = false;
}

void device_fastboot_cmd_reboot(const char *arg, void *data, unsigned sz) {
    // Give the animation a frame to notice it's done before turning the ring
    // off, otherwise it would just paint over us on its way out.
    led_ring_animating = false;
    mdelay(LED_FRAME_MS * 2);

    led_ring_off();
}