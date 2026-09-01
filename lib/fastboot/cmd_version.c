//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>

void cmd_version(const char* arg, void* data, unsigned sz) {
#ifndef CONFIG_EXCLUDE_BRANDING
    char buffer[64];
    npf_snprintf(buffer, sizeof(buffer), "kaeru v%s", KAERU_VERSION);
    fastboot_info(buffer);
    fastboot_okay("");
    print_kaeru_info(video_printf);
#else
    (void)arg;
    (void)data;
    (void)sz;
#endif
}

#ifndef CONFIG_EXCLUDE_BRANDING
FASTBOOT_CMD(version, "oem kaeru-version", cmd_version, 1);
#endif
