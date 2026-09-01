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

void fastboot_info(const char* reason);
void fastboot_fail(const char* reason);
void fastboot_okay(const char* reason);
void fastboot_register(const char* prefix, void (*handle)(const char* arg, void* data, unsigned sz),
                       unsigned char security_enabled);
void fastboot_publish(const char* name, const char* value);

#if defined(CONFIG_FASTBOOT_CMDLIST_ADDRESS) && CONFIG_FASTBOOT_CMDLIST_ADDRESS
void cmd_help(const char* arg, void* data, unsigned sz);
#endif

#ifdef CONFIG_FASTBOOT_MEM_COMMAND
bool mem_region_find(const char* name, uint32_t* base, uint32_t* size) __attribute__((weak));
void cmd_mem(const char* arg, void* data, unsigned sz);
#endif

#ifdef CONFIG_IDMELIB_SUPPORT
void cmd_idme(const char* arg, void* data, unsigned sz);
#endif