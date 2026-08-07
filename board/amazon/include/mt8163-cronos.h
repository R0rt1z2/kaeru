//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#define BOARD_NAME                      "Echo Show 5 2nd Gen (2021)"

#define FASTBOOT_INIT_PRINTF_CALL_ADDR  0x4BD28C1C

#define FB_CMD_FLASH_FUNC_ADDR          0x4BD2AE24
#define FB_CMD_ERASE_FUNC_ADDR          0x4BD2AE74

#define FB_REGISTER_FLASH_ADDR          0x4BD28CB2
#define FB_REGISTER_ERASE_ADDR          0x4BD28CC4

#define VIDEO_SET_CURSOR_FUNC_ADDR      0x4BD2C3B0
#define VIDEO_GET_ROWS_FUNC_ADDR        0x4BD2C3E8

#define FB_VIDEO_CALL_ADDRS             0x4BD2A638, 0x4BD2A6A8, 0x4BD2A6BA, \
                                        0x4BD2A700, 0x4BD2AAEE, 0x4BD2AD46, \
                                        0x4BD2AD7A
#define FB_VIDEO_TAIL_CALL_ADDR         0x4BD2A61A

#define VERITY_CMDLINE_CHECK_ADDR       0x4BD29A6A
#define VERITY_CMDLINE_DISABLED_ADDR    0x4BD29A90

#define KERNEL_64BIT_FLAG_ADDR          0x4BD701B8
#define BOOTIMG_CMDLINE_PRINT_CALL_ADDR 0x4BD13348
#define KERNEL_CMDLINE_ADDR             0x4BD581A4
