#pragma once

#include <stdint.h>
#include <stddef.h>

#define USER_PART 8

#define LK_BASE 0x41e00000
#define PAYLOAD_DST 0x41e58648
#define PAYLOAD_CALLER 0x41e1b9b4

struct device_t {
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t unk4;
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, uint32_t size, uint32_t part);
};

typedef enum {
    BOOTMODE_NORMAL = 0,
    BOOTMODE_META = 1,
    BOOTMODE_RECOVERY = 2,
    BOOTMODE_FACTORY = 4,
    BOOTMODE_ADVMETA = 5,
    BOOTMODE_ATEFACT = 6,
    BOOTMODE_ALARM = 7,
    BOOTMODE_FASTBOOT = 99,
} bootmode_t;

typedef enum {
    BOOTSTATE_GREEN = 0,
    BOOTSTATE_ORANGE = 1,
    BOOTSTATE_YELLOW = 2,
    BOOTSTATE_RED = 3,
} bootstate_t;