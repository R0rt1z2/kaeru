//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stdint.h>

#define BOOTIMG_MAGIC "ANDROID!"
#define BOOTIMG_MAGIC_SZ (8)
#define BOOTIMG_NAME_SZ (16)
#define BOOTIMG_ARGS_SZ (512)
#define BOOT_EXTRA_ARGS_SIZE (1024)

#define BOOT_HEADER_VERSION_ZERO 0
#define BOOT_HEADER_VERSION_ONE 1
#define BOOT_HEADER_VERSION_TWO 2

#define BOOTIMG_HDR_SZ (0x800)

typedef struct boot_img_hdr {
    uint8_t magic[BOOTIMG_MAGIC_SZ];
    uint32_t kernel_size;  /* size in bytes */
    uint32_t kernel_addr;  /* physical load addr */
    uint32_t ramdisk_size; /* size in bytes */
    uint32_t ramdisk_addr; /* physical load addr */
    uint32_t second_size;  /* size in bytes */
    uint32_t second_addr;  /* physical load addr */
    uint32_t tags_addr;    /* physical addr for kernel tags */
    uint32_t page_size;    /* flash page size we assume */
    uint32_t header_version;
    uint32_t os_version;
    uint8_t name[BOOTIMG_NAME_SZ]; /* asciiz product name */
    uint8_t cmdline[BOOTIMG_ARGS_SZ];
    uint32_t id[8]; /* timestamp / checksum / sha1 / etc */
    uint8_t extra_cmdline[BOOT_EXTRA_ARGS_SIZE];
    uint32_t recovery_dtbo_size;   /* size of recovery dtbo image */
    uint64_t recovery_dtbo_offset; /* physical load addr */
    uint32_t header_size;          /* size of boot image header in bytes */
    uint32_t dtb_size;             /* size in bytes for DTB image */
    uint64_t dtb_addr;             /* physical load address for DTB image */
} boot_img_hdr;