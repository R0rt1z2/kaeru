//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/fastboot.h>

#if defined(CONFIG_FASTBOOT_STYLE_COMBINED)

static void (*const _send_response)(const char* status, const char* fmt, ...) =
        (void*)(CONFIG_FASTBOOT_SEND_RESPONSE_ADDRESS | 1);

static void (*const _send_info)(const char* status, const char* fmt, ...) =
        (void*)(CONFIG_FASTBOOT_SEND_INFO_ADDRESS | 1);

void fastboot_okay(const char* reason) {
    _send_response("OKAY", reason);
}

void fastboot_fail(const char* reason) {
    _send_response("FAIL", reason);
}

void fastboot_info(const char* reason) {
    _send_info("INFO", reason);
}

#elif defined(CONFIG_FASTBOOT_STYLE_STANDARD)

void fastboot_okay(const char* reason) {
    ((void (*)(const char*))(CONFIG_FASTBOOT_OKAY_ADDRESS | 1))(reason);
}

void fastboot_fail(const char* reason) {
    ((void (*)(const char*))(CONFIG_FASTBOOT_FAIL_ADDRESS | 1))(reason);
}

void fastboot_info(const char* reason) {
    ((void (*)(const char*))(CONFIG_FASTBOOT_INFO_ADDRESS | 1))(reason);
}

#else
#error "No fastboot response style selected."
#endif

void fastboot_register(const char* prefix,
                       void (*handle)(const char* arg, void* data, unsigned sz),
                       unsigned char security_enabled) {
    ((void (*)(const char*, void (*)(const char*, void*, unsigned),
               unsigned char))(CONFIG_FASTBOOT_REGISTER_ADDRESS | 1))(
            prefix, handle, security_enabled);
}

void fastboot_publish(const char* name, const char* value) {
    ((void (*)(const char*, const char*))(CONFIG_FASTBOOT_PUBLISH_ADDRESS | 1))(name, value);
}