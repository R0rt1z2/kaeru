//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/arm.h>
#include <lib/common.h>
#include <lib/debug.h>
#include <lib/environment.h>
#include <lib/fastboot.h>

#include <wdt/mtk_wdt.h>
#include <usbdl/mtk_usbdl.h>

const char* get_mode_string(unsigned int mode) {
#ifndef CONFIG_EXCLUDE_BRANDING
    switch (mode) {
        case 0x10:
            return "User Mode (PL0)";
        case 0x11:
            return "FIQ Mode";
        case 0x12:
            return "IRQ Mode";
        case 0x13:
            return "Supervisor Mode (PL1, Kernel/OS)";
        case 0x16:
            return "Monitor Mode (PL3, Secure Mode)";
        case 0x17:
            return "Abort Mode";
        case 0x1A:
            return "Hypervisor Mode (PL2)";
        case 0x1B:
            return "Undefined Mode";
        case 0x1F:
            return "System Mode";
        default:
            return "Unknown Mode";
    }
#else
    (void)mode;
    return "Unknown Mode";
#endif
}

void reboot_emergency(void) {
    mtk_reboot_emergency();
}

bool mtk_detect_key(unsigned short key) {
    return ((bool (*)(unsigned short))(CONFIG_MTK_DETECT_KEY_ADDRESS | 1))(key);
}

void cmdline_replace(char *cmdline, const char *param,
                     const char *old, const char *new) {
    size_t param_len = strlen(param);
    size_t old_len = strlen(old);
    size_t new_len = strlen(new);

    char *p = strstr(cmdline, param);
    if (!p)
        return;

    char *value = p + param_len;
    char *after = value + old_len;
    int diff = (int)new_len - (int)old_len;

    if (diff)
        memmove(after + diff, after, strlen(after) + 1);

    memcpy(value, new, new_len);
    printf("Patched %s: %s -> %s\n", param, old, new);
}

void print_kaeru_info(int (*out)(const char *, ...)) {
#ifndef CONFIG_EXCLUDE_BRANDING
    unsigned int sp, lr, pe, vbar;

    READ_SP(sp);
    READ_LR(lr);
    READ_CPSR(pe);
    READ_VBAR(vbar);

    pe &= 0x1F;

    out(" _                         \n"
        "| | ____ _  ___ _ __ _   _ \n"
        "| |/ / _` |/ _ \\ '__| | | |\n"
        "|   < (_| |  __/ |  | |_| |\n"
        "|_|\\_\\__,_|\\___|_|   \\__,_| v%s (%s)\n\n",
        KAERU_VERSION, ARM_MODE(lr));
    out("********************************************************************\n");
    out(" Copyright (C) 2023-2026 KAERU Labs, S.L.\n");
    out(" SPDX-License-Identifier: AGPL-3.0-or-later\n\n");
    out(" Developed by Roger Ortiz <me@r0rt1z2.com> and\n");
    out("              Mateo De la Hoz <me@antiengineer.com>\n");
    out("              Shomy <git@itssho.my>\n\n");
    out(" !!! WARNING !!!\n");
    out(" THIS IS A FREE TOOL. IF YOU PAID FOR IT, YOU HAVE BEEN SCAMMED.\n");
    out(" THIS TOOL IS PROVIDED AS-IS WITHOUT WARRANTY OF ANY KIND.\n");
    out(" USE AT YOUR OWN RISK.\n\n");
    out(" Vector Base  (VBAR): 0x%08x\n", vbar);
    out(" Stack Pointer  (SP): 0x%08x\n", sp);
    out(" Link Register  (LR): 0x%08x\n", lr);
    out(" Processor Mode (PE): %s\n", get_mode_string(pe));
    out("********************************************************************\n\n");
#else
    (void)out;
#endif
}

void cmd_version(const char* arg, void* data, unsigned sz) {
#ifndef CONFIG_EXCLUDE_BRANDING
    char buffer[64];
    npf_snprintf(buffer, sizeof(buffer), "kaeru v%s", KAERU_VERSION);
    fastboot_info(buffer);
    print_kaeru_info(video_printf);
    fastboot_okay("");
#else
    (void)arg;
    (void)data;
    (void)sz;
#endif
}

void __attribute__((weak)) common_early_init(void) {
#ifndef CONFIG_EXCLUDE_BRANDING
    fastboot_publish("kaeru-version", KAERU_VERSION);
    fastboot_register("oem kaeru-version", cmd_version, 1);
#else
#warning "Branding is excluded, you are not allowed to share copies of this image."
#endif

#ifdef CONFIG_ENVIRONMENT_SUPPORT
    fastboot_register("oem env", cmd_env, 1);
#endif
}