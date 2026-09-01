//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <stdbool.h>

#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/string.h>

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

FASTBOOT_CMD(help, "oem help", cmd_help, 1);
