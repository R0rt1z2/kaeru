//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/fastboot.h>

void fastboot_info(const char* reason) {
#ifdef CONFIG_USE_FASTBOOT_ACK
    ((void (*)(const char* type, const char* reason))(CONFIG_FASTBOOT_ACK_ADDRESS | 1))("INFO", reason);
#else
    ((void (*)(const char* reason))(CONFIG_FASTBOOT_INFO_ADDRESS | 1))(reason);
#endif
}

void fastboot_fail(const char* reason) {
#ifdef CONFIG_USE_FASTBOOT_ACK
    ((void (*)(const char* type, const char* reason))(CONFIG_FASTBOOT_ACK_ADDRESS | 1))("FAIL", reason);
#else
    ((void (*)(const char* reason))(CONFIG_FASTBOOT_FAIL_ADDRESS | 1))(reason);
#endif
}

void fastboot_okay(const char* reason) {
#ifdef CONFIG_USE_FASTBOOT_ACK
    ((void (*)(const char* type, const char* reason))(CONFIG_FASTBOOT_ACK_ADDRESS | 1))("OKAY", reason);
#else
    ((void (*)(const char* reason))(CONFIG_FASTBOOT_OKAY_ADDRESS | 1))(reason);
#endif
}

void fastboot_register(const char* prefix, void (*handle)(const char* arg, void* data, unsigned sz),
                       unsigned char security_enabled) {
    ((void (*)(const char* prefix, void (*handle)(const char* arg, void* data, unsigned sz),
               unsigned char security_enabled))(CONFIG_FASTBOOT_REGISTER_ADDRESS | 1))(
            prefix, handle, security_enabled);
}

void fastboot_publish(const char* name, const char* value) {
    ((void (*)(const char* name, const char* value))(CONFIG_FASTBOOT_PUBLISH_ADDRESS | 1))(name,
                                                                                           value);
}
