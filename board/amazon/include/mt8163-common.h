//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <board_ops.h>

#if defined(CONFIG_AMAZON_CHECKERS)
#include "mt8163-checkers.h"
#elif defined(CONFIG_AMAZON_CROWN)
#include "mt8163-crown.h"
#elif defined(CONFIG_AMAZON_CRONOS)
#include "mt8163-cronos.h"
#else
#error "Invalid device selection"
#endif

#define UNLOCK_CHECK_PATTERN 0xB510, 0xB0C0, 0x2100, 0xF44F, 0x7280, 0x4668

static inline void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(VIDEO_SET_CURSOR_FUNC_ADDR|1))(row, col);
}

static inline int video_get_rows(void) {
    return ((int (*)(void))(VIDEO_GET_ROWS_FUNC_ADDR|1))();
}
