//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <lib/bootmode.h>

struct misc_message {
    char command[32];
    char status[32];
    char recovery[1024];
};

bootmode_t misc_command_to_bootmode(const char* command);