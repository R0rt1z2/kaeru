//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/cache.h>
#include <lib/bootargs.h>
#include <lib/common.h>
#include <lib/debug.h>

static bool do_cmdline_replace(char *cmdline, const char *param,
                               const char *old, const char *new) {
    if (!cmdline)
        return false;

    if (strnlen(cmdline, CMDLINE_LEN) == CMDLINE_LEN)
        return false;

    size_t param_len = strlen(param);
    size_t old_len   = strlen(old);
    size_t new_len   = strlen(new);

    char *p = strstr(cmdline, param);
    if (!p)
        return false;

    char *value = p + param_len;
    if (strncmp(value, old, old_len) != 0)
        return false;

    char *after     = value + old_len;
    size_t tail_len = strlen(after);

    if ((size_t)(value - cmdline) + new_len + tail_len + 1 > CMDLINE_LEN)
        return false;

    int diff = (int)new_len - (int)old_len;
    if (diff)
        memmove(after + diff, after, tail_len + 1);

    memcpy(value, new, new_len);
    printf("Patched %s: %s -> %s\n", param, old, new);
    return true;
}

bool cmdline_replace(char *cmdline, const char *param,
                     const char *old, const char *new) {
    uint32_t prev = enable_unaligned();
    bool ret = do_cmdline_replace(cmdline, param, old, new);
    restore_unaligned(prev);
    return ret;
}