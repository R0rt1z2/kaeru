//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <roger@r0rt1z2.com>
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

void fastboot_register(const char* prefix,
                       void (*handle)(const char* arg, void* data, unsigned sz),
                       unsigned char security_enabled) {
    (void)prefix;
    (void)handle;
    (void)security_enabled;
}

void fastboot_publish(const char* name, const char* value) {
    (void)name;
    (void)value;
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

#elif defined(CONFIG_FASTBOOT_STYLE_HYBRID)

static void (*const _send_response)(const char* status, const char* fmt, ...) =
        (void*)(CONFIG_FASTBOOT_SEND_RESPONSE_ADDRESS | 1);

#if CONFIG_FASTBOOT_SETUP_ADDRESS != 0
static void (*const _setup_response)(int arg) =
        (void*)(CONFIG_FASTBOOT_SETUP_ADDRESS | 1);
#endif

void fastboot_okay(const char* reason) {
    // Some LK builds need the response state reset before each call.
    // This mirrors fastboot_continue's pattern of calling a setup
    // function between response sends.
#if CONFIG_FASTBOOT_SETUP_ADDRESS != 0
    _setup_response(1);
#endif
    // Use the dedicated okay wrapper that hardcodes "OKAY" as the
    // type and calls send_response internally.
    ((void (*)(const char*))(CONFIG_FASTBOOT_OKAY_ADDRESS | 1))(reason);
}

void fastboot_fail(const char* reason) {
#if CONFIG_FASTBOOT_SETUP_ADDRESS != 0
    _setup_response(1);
#endif
    _send_response("FAIL", reason);
}

void fastboot_info(const char* reason) {
#if CONFIG_FASTBOOT_SETUP_ADDRESS != 0
    _setup_response(1);
#endif
    _send_response("INFO", reason);
}

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

#else
#error "No fastboot response style selected."
#endif