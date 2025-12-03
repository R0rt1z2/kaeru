//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stdint.h>

#define BLOCK_SIZE 512

#define MISC_PAGES            3
#define MISC_COMMAND_PAGE     1

#define BOOT0_PART 1
#define USER_PART  8

#ifdef CONFIG_LEGACY_LK

#ifdef CONFIG_USE_PMT_PARTITION

typedef struct {
    char *name;
    unsigned long blknum;
    unsigned long flags;
    unsigned long startblk;
    unsigned int part_id;
} part_t;

#else

typedef struct {
    unsigned long start_sect;
    unsigned long nr_sects;
    unsigned int part_id;
    char *name;
    void *info;
} part_t;

#endif

struct device_t {
    uint32_t init;
    uint32_t id;
    void *blkdev;
    int (*init_dev)(int id);
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, size_t size, uint32_t part);
};

#endif

struct misc_message {
    char command[32];
    char status[32];
    char recovery[1024];
};