/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// Lock State
enum {
    LKS_DEFAULT    = 0x01,
    LKS_MP_DEFAULT = 0x02,
    LKS_UNLOCK     = 0x03,
    LKS_LOCK       = 0x04,
    LKS_VERIFIED   = 0x05,
    LKS_CUSTOM     = 0x06,
};

// Lock Critical State
enum {
    LKCS_UNLOCK = 0x01,
    LKCS_LOCK   = 0x02,
};

// Secure boot runtime switch
enum {
    SBOOT_RUNTIME_OFF = 0,
    SBOOT_RUNTIME_ON  = 1,
};

// dm-verity state
enum {
    DM_VERITY_OK  = 0,
    DM_VERITY_ERR = 1,
};

#define SECCFG_SIZE        0x3C
#define SECCFG_START_MAGIC 0x4D4D4D4D
#define SECCFG_END_MAGIC   0x45454545

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t lock_state;
    uint32_t dm_verity;
    uint32_t sboot_runtime;
    uint32_t end_magic;
    uint8_t  hash[32];
    uint8_t  reserved[0x200 - SECCFG_SIZE];
} __attribute__((packed)) SecCfgV4;

#ifdef CONFIG_SEJ_SUPPORT
bool seccfg_is_valid(const SecCfgV4 *cfg);
bool seccfg_is_unlocked(const SecCfgV4 *cfg);
void seccfg_seal(SecCfgV4 *cfg);
int seccfg_apply_unlock(SecCfgV4 *cfg);
#endif
