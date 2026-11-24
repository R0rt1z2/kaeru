//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#define BOARD_NAME                      "Fire 7 (2022)"

#define KEY_VOLUME_UP                   0
#define KEY_VOLUME_DOWN                 3

#define FASTBOOT_INIT_PRINTF_CALL_ADDR  0x5602EE1E

#define FB_CMD_FLASH_FUNC_ADDR          0x56032324
#define FB_CMD_ERASE_FUNC_ADDR          0x56032328
#define FB_CMD_REBOOT_FUNC_ADDR         0x5602F9F4

#define FB_REGISTER_GETVAR_ADDR         0x5602EE9E
#define FB_REGISTER_FLASH_ADDR          0x5602EF2A
#define FB_REGISTER_ERASE_ADDR          0x5602EF42
#define FB_REGISTER_REBOOT_ADDR         0x5602EF70
#define FB_REGISTER_REBOOT_BOOTLOADER_ADDR 0x5602EF88
#define FB_REGISTER_REBOOT_RECOVERY_ADDR 0x5602EFA0
#define FB_REGISTER_REBOOT_FASTBOOT_ADDR 0x5602EFB8

#define LK_IDME_REGISTER_FUNC_ADDR      0x56000844

#define VIDEO_SET_CURSOR_FUNC_ADDR      0x560347DC
#define VIDEO_GET_ROWS_FUNC_ADDR        0x56034814

#define FB_VIDEO_CALL_ADDRS             0x56031714, 0x560317CE, 0x56031906, \
                                        0x56031CCA, 0x56031F20
#define FB_VIDEO_TAIL_CALL_ADDRS        0x560316F4
#define FB_CLEAN_SCREEN_CALL_ADDRS      0x56031888, 0x560318A2

#define SHOW_BOOT_MODE_CALL_ADDR        0x5600F322

// This LK verifies with libavb instead of Amazon's own image checks, so
// there is no boot state global to pin and no orange warning screen.
#define VERITY_CHECK_FUNC_ADDR          0x560298B8
#define SELINUX_CMDLINE_FUNC_ADDR       0x560298E0

#define KERNEL_64BIT_FLAG_ADDR          0x560AAF48
#define BOOTOPT_PARSE_CALL_ADDR         0x56030432

#define BOOT_MODE_SELECT_CALL_ADDR      0x5600F1E8
#define BOOT_ARG_PTR_ADDR               0x560B2A28
#define RTC_READ_FUNC_ADDR              0x56012338
#define RTC_MARK_FAST_BOOT_FUNC_ADDR    0x560127E4
#define MTK_DETECT_PMIC_JUST_RST_ADDR   0x560103C8
#define KPOC_DETECT_FUNC_ADDR           0x56027BE8

#define LOAD_DTB_FUNC_ADDR              0x56030330
#define RECOVERY_DTBO_APPLIED_FUNC_ADDR 0x5603DBB8
