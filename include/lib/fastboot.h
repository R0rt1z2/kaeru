//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FASTBOOT_INFO_MAX 59

struct fastboot_cmd {
    struct fastboot_cmd* next;
    const char* prefix;
    unsigned int prefix_len;
    int allowed_when_security_on;
    int forbidden_when_lock_on;
    void (*handle)(const char* arg, void* data, unsigned int sz);
};

struct fastboot_command {
    const char* prefix;
    void (*handle)(const char* arg, void* data, unsigned sz);
    int security;
};

#define FASTBOOT_CMD(id, str, fn, sec)                                      \
    static const struct fastboot_command __fastboot_cmd_##id                \
            __attribute__((used, section(".fastboot_cmds"), aligned(4))) = { \
                    .prefix = (str),                                        \
                    .handle = (fn),                                         \
                    .security = (sec),                                      \
            }

void fastboot_register_commands(void);

void fastboot_info(const char* reason);
void fastboot_fail(const char* reason);
void fastboot_okay(const char* reason);
void fastboot_register(const char* prefix, void (*handle)(const char* arg, void* data, unsigned sz),
                       unsigned char security_enabled);
void fastboot_publish(const char* name, const char* value);

void cmd_version(const char* arg, void* data, unsigned sz);

bool mem_region_find(const char* name, uint32_t* base, uint32_t* size) __attribute__((weak));