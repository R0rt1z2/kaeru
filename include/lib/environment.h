//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <lib/common.h>
#include <lib/string.h>

#ifdef CONFIG_KAERU_ENV
#define ENV_PARTITION  CONFIG_KAERU_ENV_PARTITION
#define ENV_OFFSET     CONFIG_KAERU_ENV_OFFSET
#define ENV_SIZE       CONFIG_KAERU_ENV_SIZE

#define ENV_SIG        "ENV_v1"
#define ENV_SIG_SIZE   8
#define ENV_CKSUM_SIZE 4

#define ENV_DATA_OFF   ENV_SIG_SIZE
#define ENV_CKSUM_OFF  (ENV_SIZE - ENV_CKSUM_SIZE)
#define ENV_SIG2_OFF   (ENV_CKSUM_OFF - ENV_SIG_SIZE)
#define ENV_DATA_SIZE  (ENV_SIG2_OFF - ENV_DATA_OFF)

typedef char env_size_must_hold_a_block[
    (ENV_SIZE >= 0x200 && ENV_SIZE % 0x200 == 0) ? 1 : -1];
#endif

char *get_env(char *name);
int set_env(char *name, char *value);
