//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#define BLOCK_SIZE 512

#define MISC_PAGES            3
#define MISC_COMMAND_PAGE     1

#define PART_META_INFO_NAMELEN  64
#define PART_META_INFO_UUIDLEN  16

struct part_meta_info {
    uint8_t name[PART_META_INFO_NAMELEN];
    uint8_t uuid[PART_META_INFO_UUIDLEN];
};

typedef struct part_t {
    unsigned long  start_sect;
    unsigned long  nr_sects;
    unsigned int part_id;
    char *name;
    struct part_meta_info *info;
} part_t;

struct device_t {
    uint32_t init;
    uint32_t id;
    void *blkdev;
    int (*init_dev) (int id);
    size_t (*read)(struct device_t *dev, uint64_t dev_addr, void *dst, uint32_t size, uint32_t part);
    size_t (*write)(struct device_t *dev, void *src, uint64_t block_off, size_t size, uint32_t part);
};

struct misc_message {
    char command[32];
    char status[32];
    char recovery[1024];
};