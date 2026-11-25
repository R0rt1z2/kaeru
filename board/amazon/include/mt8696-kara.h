//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#define BOARD_NAME                         "Fire TV Stick 4K Max (1st Gen)"

#define BOOT_ARG_PTR_ADDR                  0x41E76A54
#define BOOT_ARG_USB_EXTCONN_OFF           0x17C
#define BOOT_ARG_Y_CABLE_OFF               0x17D
#define BOOT_ARG_BOOT_REASON_OFF           0x120

#define UNLOCK_CHECK_FUNC_ADDR             0x41E01E70

#define IMAGE_VERIFY_FUNC_ADDR             0x41E26DAC

#define FASTBOOT_INIT_PRINTF_CALL_ADDR     0x41E29EC2

#define FB_CMD_FLASH_FUNC_ADDR             0x41E2B1BC
#define FB_CMD_ERASE_FUNC_ADDR             0x41E2B310
#define FB_CMD_REBOOT_FUNC_ADDR            0x41E2A450
#define FB_CMD_REBOOT_BOOTLOADER_FUNC_ADDR 0x41E2A474

#define FB_REGISTER_GETVAR_ADDR            0x41E29F0C
#define FB_REGISTER_FLASH_ADDR             0x41E29F50
#define FB_REGISTER_ERASE_ADDR             0x41E29F62
#define FB_REGISTER_REBOOT_ADDR            0x41E29F8E
#define FB_REGISTER_REBOOT_BOOTLOADER_ADDR 0x41E29FA0
#define FB_REGISTER_RELOCK_ADDRS           0x41E2A05A
#define LK_IDME_REGISTER_CALL_ADDR         0x41E29798

#define VIDEO_SET_CURSOR_FUNC_ADDR         0x41E2BC7C
#define VIDEO_GET_ROWS_FUNC_ADDR           0x41E2BCB8
#define VIDEO_ROW_ADDR                     0x41E76274

#define CMDLINE_GET_FUNC_ADDR              0x41E286B0
#define CMDLINE_APPEND_FUNC_ADDR           0x41E286BC
#define CMDLINE_GET_CALL_ADDR              0x41E2959E

#define BOOT_HDR_PTR_ADDR                  0x41E741C4
#define BOOT_HDR_READ_SIZE                 0x260
#define BOOTIMG_SIZE_ADDR                  0x41E6D800
#define BOOTIMG_STAGE_BASE                 0x60000000
#define BOOT_LINUX_FUNC_ADDR               0x41E29458
#define BOOT_LINUX_CALL_ADDR               0x41E295BA
#define BOOT_LINUX_MACH_TYPE               0x21F8

#define KERNEL_64BIT_FLAG_ADDR             0x41E741CC
#define KERNEL_64BIT_ALIGN                 0x80000
