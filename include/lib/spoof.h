//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <lib/string.h>
#include <lib/environment.h>
#include <lib/lock_state.h>
#include <lib/fastboot.h>

int is_spoofing_enabled(void);
int get_lock_state(uint32_t *lock_state);
void cmd_spoof_bootloader_lock(const char *arg, void *data, unsigned sz);