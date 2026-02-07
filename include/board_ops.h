//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <arch/arm.h>
#include <lib/app.h>
#include <lib/bootimg.h>
#include <lib/bootmode.h>
#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/lock_state.h>
#include <lib/string.h>
#include <lib/storage.h>
#include <lib/thread.h>

#ifdef CONFIG_FRAMEBUFFER_SUPPORT
#include <lib/framebuffer.h>
#endif

#ifdef CONFIG_LIBSEJ_SUPPORT
#include <lib/sej.h>
#endif

#ifdef CONFIG_SPOOF_SUPPORT
#include <lib/spoof.h>
#endif

void board_early_init(void);
void board_late_init(void);
