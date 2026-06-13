//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stdbool.h>

#define CMDLINE_LEN   2048
#define FDT_BUFF_SIZE CMDLINE_LEN

bool cmdline_replace(char *cmdline, const char *param,
                     const char *old, const char *new);