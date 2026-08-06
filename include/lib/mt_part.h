//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stddef.h>
#include <stdint.h>

#define BLOCK_SIZE 512

#define BOOT0_PART 1
#define USER_PART  8

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

static inline struct device_t* mt_part_get_device(void) {
    return ((struct device_t* (*)(void))(CONFIG_MT_PART_GET_DEVICE_ADDRESS | 1))();
}

#ifdef CONFIG_MT_PART_GET_PARTITION_ADDRESS
static inline part_t* mt_part_get_partition(const char* name) {
    return ((part_t* (*)(const char*))(CONFIG_MT_PART_GET_PARTITION_ADDRESS | 1))(name);
}
#endif

static inline uint64_t mt_part_offset(const part_t* part) {
#ifdef CONFIG_USE_PMT_PARTITION
    return (uint64_t)part->startblk * BLOCK_SIZE;
#else
    return (uint64_t)part->start_sect * BLOCK_SIZE;
#endif
}

static inline uint64_t mt_part_size(const part_t* part) {
#ifdef CONFIG_USE_PMT_PARTITION
    return (uint64_t)part->blknum * BLOCK_SIZE;
#else
    return (uint64_t)part->nr_sects * BLOCK_SIZE;
#endif
}
