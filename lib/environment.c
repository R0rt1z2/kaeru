
//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/environment.h>

char *get_env(char *name) {
    return ((char *(*)(char *))(CONFIG_GET_ENV_ADDRESS | 1))(name);
}

int set_env(char *name, char *value) {
    return ((int (*)(char *, char *))(CONFIG_SET_ENV_ADDRESS | 1))(name, value);
}