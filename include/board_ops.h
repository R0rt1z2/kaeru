//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <arch/arm.h>
#include <lib/bootimg.h>
#include <lib/bootmode.h>
#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/recovery.h>
#include <lib/string.h>
#include <lib/thread.h>

void board_early_init(void);
void board_late_init(void);