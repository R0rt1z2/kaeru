//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <lib/string.h>
#include <lib/environment.h>
#include <lib/security/seccfg.h>
#include <lib/fastboot.h>

int is_spoofing_enabled(void);
int get_lock_state(uint32_t *lock_state);
int sec_usbdl_enabled(void);
unsigned int seclib_sec_boot_enabled(unsigned int);
unsigned get_unlocked_status(void);
void cmd_spoof_bootloader_lock(const char *arg, void *data, unsigned sz);