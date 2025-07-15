//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#define MISC_PAGES            3
#define MISC_COMMAND_PAGE     1

struct misc_message {
    char command[32];
    char status[32];
    char recovery[1024];
};