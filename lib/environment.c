//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
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

static int parse_env_args(const char *arg, char *key, size_t key_sz,
                          char *val, size_t val_sz) {
    int count = 0;

    while (*arg == ' ') arg++;
    if (*arg == '\0') return 0;

    size_t i = 0;
    while (*arg && *arg != ' ' && i < key_sz - 1)
        key[i++] = *arg++;
    key[i] = '\0';
    count = 1;

    while (*arg == ' ') arg++;
    if (*arg == '\0') return count;

    i = 0;
    while (*arg && i < val_sz - 1)
        val[i++] = *arg++;
    val[i] = '\0';
    count = 2;

    return count;
}

static int is_env_key_valid(const char *key) {
    if (!key || *key == '\0') return 0;

    for (const char *p = key; *p; p++) {
        char c = *p;
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '.' || c == '-')
            continue;
        return 0;
    }

    return 1;
}

static int match_subcmd(const char *arg, const char *cmd, int len) {
    return !strncmp(arg, cmd, len) && (arg[len] == ' ' || arg[len] == '\0');
}

void cmd_env(const char *arg, void *data, unsigned sz) {
    char key[ENV_KEY_MAX_LEN] = {0};
    char val[ENV_VAL_MAX_LEN] = {0};
    char msg[ENV_MSG_MAX_LEN] = {0};

    while (*arg == ' ') arg++;

    if (strlen(arg) > ENV_MSG_MAX_LEN) {
        fastboot_fail("Argument too long");
        return;
    }

    if (match_subcmd(arg, "get", 3)) {
        arg += 3;
        if (parse_env_args(arg, key, sizeof(key), val, sizeof(val)) < 1) {
            fastboot_fail("Usage: fastboot oem env get <key>");
            return;
        }

        if (!is_env_key_valid(key)) {
            fastboot_fail("Invalid key name");
            return;
        }

        char *result = get_env(key);
        if (!result) {
            npf_snprintf(msg, sizeof(msg), "'%s' not found", key);
            fastboot_fail(msg);
            return;
        }

        npf_snprintf(msg, sizeof(msg), "%s=%s", key, result);
        fastboot_info(msg);
        fastboot_okay("");
        return;
    }

    if (match_subcmd(arg, "set", 3)) {
        arg += 3;
        if (parse_env_args(arg, key, sizeof(key), val, sizeof(val)) < 2) {
            fastboot_fail("Usage: fastboot oem env set <key> <value>");
            return;
        }

        if (!is_env_key_valid(key)) {
            fastboot_fail("Invalid key name");
            return;
        }

        if (set_env(key, val) < 0) {
            npf_snprintf(msg, sizeof(msg), "Failed to set '%s'", key);
            fastboot_fail(msg);
            return;
        }

        npf_snprintf(msg, sizeof(msg), "%s=%s", key, val);
        fastboot_info(msg);
        fastboot_okay("");
        return;
    }

    fastboot_info("kaeru environment variable control");
    fastboot_info("");
    fastboot_info("Commands:");
    fastboot_info("  get <key>         - Get a variable");
    fastboot_info("  set <key> <value> - Set a variable");
    fastboot_fail("Usage: fastboot oem env <get|set>");
}