//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

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