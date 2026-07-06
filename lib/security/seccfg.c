/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
 */

#include <lib/security/seccfg.h>
#include <lib/security/sej.h>
#include <lib/crypto/sha256.h>
#include <lib/string.h>

bool seccfg_is_valid(const SecCfgV4 *cfg) {
    return cfg->magic == SECCFG_START_MAGIC &&
           cfg->end_magic == SECCFG_END_MAGIC;
}

bool seccfg_is_unlocked(const SecCfgV4 *cfg) {
    return cfg->lock_state == LKS_UNLOCK;
}

void seccfg_seal(SecCfgV4 *cfg) {
    uint8_t hash[32] __attribute__((aligned(16)));

    sha256_hash(hash, (const uint8_t *)cfg, 0x1C);

    sej_param_t params = {0};
    params.key_id = AES_SW_KEY;
    params.key_sz = AES_KEY_256;
    params.mode = AES_CBC_MODE;
    params.legacy = false;
    params.length = sizeof(hash);
    params.anti_clone = true;
    params.encrypt = AES_ENC;
    sp_sej_enc(hash, hash, params);

    memcpy(cfg->hash, hash, sizeof(hash));
}

int seccfg_apply_unlock(SecCfgV4 *cfg) {
    if (!seccfg_is_valid(cfg))
        return -1;

    if (seccfg_is_unlocked(cfg))
        return 0;

    cfg->version = 4;
    cfg->size = SECCFG_SIZE; /* shouldn't be needed, but YMMV */
    cfg->lock_state = LKS_UNLOCK;
    cfg->end_magic = SECCFG_END_MAGIC;
    seccfg_seal(cfg);

    return 1;
}
