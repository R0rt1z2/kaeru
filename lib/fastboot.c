//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <stdbool.h>

#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/string.h>

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
    ((void (*)(const char*, void (*)(const char*, void*, unsigned), unsigned,
               unsigned))(CONFIG_FASTBOOT_REGISTER_ADDRESS | 1))(prefix, handle,
                                                                 security_enabled, 0);
}

void fastboot_publish(const char* name, const char* value) {
    ((void (*)(const char*, const char*))(CONFIG_FASTBOOT_PUBLISH_ADDRESS | 1))(name, value);
}

#else
#error "No fastboot response style selected."
#endif

#if defined(CONFIG_FASTBOOT_CMDLIST_ADDRESS) && CONFIG_FASTBOOT_CMDLIST_ADDRESS

static struct fastboot_cmd* fastboot_cmdlist(void) {
    return *(struct fastboot_cmd**)CONFIG_FASTBOOT_CMDLIST_ADDRESS;
}

static bool cmd_is_shadowed(const struct fastboot_cmd* cmd) {
    for (const struct fastboot_cmd* p = fastboot_cmdlist(); p && p != cmd; p = p->next) {
        if (p->prefix && strcmp(p->prefix, cmd->prefix) == 0) {
            return true;
        }
    }

    return false;
}

void cmd_help(const char* arg, void* data, unsigned sz) {
    const struct fastboot_cmd* cmd = fastboot_cmdlist();
    if (!cmd) {
        fastboot_fail("Command list is not available");
        return;
    }

    char line[FASTBOOT_INFO_MAX + 1];

    fastboot_info("Available oem commands:");
    for (; cmd; cmd = cmd->next) {
        if (!cmd->prefix || strncmp(cmd->prefix, "oem ", 4) != 0 || cmd_is_shadowed(cmd)) {
            continue;
        }

        npf_snprintf(line, sizeof(line), "  %s", cmd->prefix);
        fastboot_info(line);
    }

    fastboot_okay("");
}
#endif