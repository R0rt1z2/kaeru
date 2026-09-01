//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/environment.h>
#include <lib/fastboot.h>
#include <lib/common.h>

#define ENV_KEY_MAX_LEN 64
#define ENV_VAL_MAX_LEN 256
#define ENV_MSG_MAX_LEN (ENV_KEY_MAX_LEN + ENV_VAL_MAX_LEN + 16)

char *get_env(char *name) {
    return ((char *(*)(char *))(CONFIG_GET_ENV_ADDRESS | 1))(name);
}

int set_env(char *name, char *value) {
    return ((int (*)(char *, char *))(CONFIG_SET_ENV_ADDRESS | 1))(name, value);
}
