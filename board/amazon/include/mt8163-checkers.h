//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#define BOARD_NAME                      "Echo Show 5 1st Gen (2019)"

#define FASTBOOT_INIT_PRINTF_CALL_ADDR  0x4BD28AE0

#define FB_CMD_FLASH_FUNC_ADDR          0x4BD2ACE8
#define FB_CMD_ERASE_FUNC_ADDR          0x4BD2AD38

#define FB_REGISTER_FLASH_ADDR          0x4BD28B76
#define FB_REGISTER_ERASE_ADDR          0x4BD28B88

#define VIDEO_SET_CURSOR_FUNC_ADDR      0x4BD2C308
#define VIDEO_GET_ROWS_FUNC_ADDR        0x4BD2C340

#define VERITY_CMDLINE_CHECK_ADDR       0x4BD2992E
#define VERITY_CMDLINE_DISABLED_ADDR    0x4BD29954
