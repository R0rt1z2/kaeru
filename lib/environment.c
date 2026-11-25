//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/environment.h>
#include <lib/fastboot.h>
#include <lib/common.h>

#ifndef CONFIG_KAERU_ENV
char *get_env(char *name) {
    return ((char *(*)(char *))(CONFIG_GET_ENV_ADDRESS | 1))(name);
}

int set_env(char *name, char *value) {
    return ((int (*)(char *, char *))(CONFIG_SET_ENV_ADDRESS | 1))(name, value);
}
#else
#include <lib/storage.h>

static const char env_sig[ENV_SIG_SIZE] __attribute__((aligned(4))) = ENV_SIG;

static struct {
    uint8_t block[ENV_SIZE] __attribute__((aligned(64)));
    bool loaded;
} env;

static char *env_data(void) {
    return (char *)&env.block[ENV_DATA_OFF];
}

static uint32_t env_cksum(void) {
    uint32_t sum = 0;

    for (size_t i = 0; i < ENV_DATA_SIZE; i++)
        sum += env.block[ENV_DATA_OFF + i];

    return sum;
}

static bool env_is_signed(void) {
    return !memcmp(&env.block[0], ENV_SIG, sizeof(ENV_SIG)) &&
           !memcmp(&env.block[ENV_SIG2_OFF], ENV_SIG, sizeof(ENV_SIG));
}

static void env_sign(void) {
    memcpy(&env.block[0], env_sig, ENV_SIG_SIZE);
    memcpy(&env.block[ENV_SIG2_OFF], env_sig, ENV_SIG_SIZE);
}

static void env_clear(void) {
    memset(env.block, 0, sizeof(env.block));
    env_sign();
}

static const struct part_info *env_partition(void) {
    const struct part_info *part = storage_part_find(ENV_PARTITION);

    if (!part)
        printf("Failed to find the %s partition\n", ENV_PARTITION);

    return part;
}

static bool env_load(void) {
    if (env.loaded)
        return true;

    const struct part_info *part = env_partition();
    if (!part)
        return false;

    if (storage_part_read(part, env.block, ENV_OFFSET, sizeof(env.block))
        != (ssize_t)sizeof(env.block)) {
        printf("Failed to read the environment from %s\n", ENV_PARTITION);
        return false;
    }

    if (!env_is_signed() || env_cksum() != LE32(&env.block[ENV_CKSUM_OFF])) {
        printf("No valid environment in %s, starting an empty one\n",
               ENV_PARTITION);
        env_clear();
    }

    env.loaded = true;
    return true;
}

static bool env_save(void) {
    const struct part_info *part = env_partition();
    if (!part)
        return false;

    env_sign();

    uint32_t sum = env_cksum();
    for (size_t i = 0; i < ENV_CKSUM_SIZE; i++)
        env.block[ENV_CKSUM_OFF + i] = (uint8_t)(sum >> (8 * i));

    if (storage_part_write(part, env.block, ENV_OFFSET, sizeof(env.block))
        != (ssize_t)sizeof(env.block)) {
        printf("Failed to write the environment to %s\n", ENV_PARTITION);
        env.loaded = false;
        return false;
    }

    return true;
}

static size_t env_entry_len(size_t off) {
    return strnlen(env_data() + off, ENV_DATA_SIZE - off);
}

static size_t env_used(void) {
    size_t off = 0;

    while (off < ENV_DATA_SIZE && env_data()[off]) {
        size_t len = env_entry_len(off);

        if (off + len == ENV_DATA_SIZE)
            return ENV_DATA_SIZE;

        off += len + 1;
    }

    return off;
}

static char *env_entry(const char *name) {
    size_t nlen = strlen(name);
    size_t off = 0;

    while (off < ENV_DATA_SIZE && env_data()[off]) {
        size_t len = env_entry_len(off);

        if (off + len == ENV_DATA_SIZE)
            break;

        char *entry = env_data() + off;
        if (len > nlen && entry[nlen] == '=' && !strncmp(entry, name, nlen))
            return entry;

        off += len + 1;
    }

    return NULL;
}

static void env_drop(char *entry) {
    size_t off = (size_t)(entry - env_data());
    size_t len = env_entry_len(off) + 1;
    size_t used = env_used();

    memmove(entry, entry + len, used - off - len);
    memset(env_data() + used - len, 0, len);
}

static bool env_name_is_valid(const char *name) {
    return name && *name && !strchr(name, '=');
}

char *get_env(char *name) {
    if (!env_name_is_valid(name) || !env_load())
        return NULL;

    char *entry = env_entry(name);

    return entry ? entry + strlen(name) + 1 : NULL;
}

int set_env(char *name, char *value) {
    if (!env_name_is_valid(name) || !value || !env_load())
        return -1;

    char *entry = env_entry(name);
    if (entry && !strcmp(entry + strlen(name) + 1, value))
        return 0;

    if (entry)
        env_drop(entry);

    size_t nlen = strlen(name);
    size_t vlen = strlen(value);
    size_t used = env_used();

    if (nlen + vlen + 3 > ENV_DATA_SIZE - used) {
        printf("No room left in the environment for '%s'\n", name);
        return -1;
    }

    char *p = env_data() + used;
    memmove(p, name, nlen);
    p[nlen] = '=';
    memmove(p + nlen + 1, value, vlen);
    p[nlen + vlen + 1] = '\0';
    p[nlen + vlen + 2] = '\0';

    return env_save() ? 0 : -1;
}
#endif
