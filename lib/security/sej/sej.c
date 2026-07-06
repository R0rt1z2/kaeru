/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2025-2026 Shomy
 */

#include <arch/mmio.h>
#include <lib/string.h>
#include <lib/crypto/xor.h>
#include <lib/security/sej.h>

#define ERR_SEC_CYPHER_KEY_INVALID 0xC0020032
#define ERR_SEC_CYPHER_DATA_UNALIGNED 0xC0020033

/* Anti clone */
const uint32_t g_AC_FIXED_PATTERN[3][4] = {
    { 0x2D44BB70, 0xA744D227, 0xD0A9864B, 0x83FFC244 },
    { 0x7EC8266B, 0x43E80FB2, 0x01A6348A, 0x2067F9A0 },
    { 0x54536405, 0xD546A6B1, 0x1CC3EC3A, 0xDE377A83 }
};

// Used for seccfg, and MTEE as well?
const uint32_t g_AC_BOOTROM_IV[8] = {
	0x9ED40400, 0x00E884A1, 0xE3F083BD, 0x2F4E6D8A,
	0xFF838E5C, 0xE940A0E3, 0x8D4DECC6, 0x45FC0989
};

sej_ctx_t g_sej_ctx;

const uint8_t DEFAULT_IV[16] = {
    0x57, 0x32, 0x5A, 0x5A,
    0x12, 0x54, 0x97, 0x66,
    0x12, 0x54, 0x97, 0x66,
    0x57, 0x32, 0x5A, 0x5A
};

const uint8_t DEFAULT_KEY[32] = {
    0x25, 0xA1, 0x76, 0x3A, 0x21, 0xBC, 0x85, 0x4C,
    0xD5, 0x69, 0xDC, 0x23, 0xB4, 0x78, 0x2B, 0x63,
    0x25, 0xA1, 0x76, 0x3A, 0x21, 0xBC, 0x85, 0x4C,
    0xD5, 0x69, 0xDC, 0x23, 0xB4, 0x78, 0x2B, 0x63
};

volatile uintptr_t hacc_base = 0x1000A000;

void set_sej_base(uintptr_t base_addr) {
    hacc_base = base_addr;
}

uintptr_t get_sej_base(void) {
    return hacc_base;
}

void sej_init(void) {
    AES_IV iv;
    set_sej_base(CONFIG_SEJ_BASE);

    /* Clear Engine */
    uint32_t acon2_settings = readl(SEJ_ACON2);
    acon2_settings &= 0xFFFFEFFD | SEJ_AES_CLR;
    writel(acon2_settings, SEJ_ACON2);

    memset(&g_sej_ctx, 0, sizeof(g_sej_ctx));

    memcpy(g_sej_ctx.sw_key, DEFAULT_KEY, SEJ_AES_MAX_KEY_SZ);
    memcpy(iv.vector, DEFAULT_IV, AES_CFG_SZ);

    sej_set_iv(&iv);
    g_sej_ctx.blk_sz = AES_BLK_SZ;
    g_sej_ctx.use_custom_iv = false;
    g_sej_ctx.legacy = false;
}

uint32_t sej_set_otp(uint32_t* otp){
    for (uint32_t i = 0; i < 8; i++) {
        writel(otp[i], SEJ_SW_OTP0 + (4 * i));
    }

    return 0;
}

void sej_clear_otp(void) {
    for (int32_t i = 0; i < 8; i++) {
        writel(0, SEJ_SW_OTP0 + (4 * i));
    }
}

void sej_secinit_set_magic(void) {
    writel(SEJ_SECINIT0_MAGIC, SEJ_SECINIT0);
    writel(SEJ_SECINIT1_MAGIC, SEJ_SECINIT1);
    writel(SEJ_SECINIT2_MAGIC, SEJ_SECINIT2);
}

int sej_set_key(AES_KEY_ID id,AES_KEY_SZ key) {
    uint32_t key_sz = 0;

    if (key != AES_KEY_128 && key != AES_KEY_192 && key != AES_KEY_256)
        return ERR_SEC_CYPHER_KEY_INVALID;

    uint32_t acon_settings = readl(SEJ_ACON);
    acon_settings &= ~SEJ_AES_TYPE_MASK;
    acon_settings |= key;
    writel(acon_settings, SEJ_ACON);

    key_sz = (key << 3) + 16;

    /* Clear key */
    for(int i=0; i < 8; i++)
        writel(0, SEJ_AKEY0 + (i * 4));

    uint8_t* p_key = g_sej_ctx.hw_key;

    switch (id) {
        case AES_HW_KEY:
            g_sej_ctx.blk_sz = key_sz;

            if (g_sej_ctx.legacy) {
                /* Bind HUK */
                writel(readl(SEJ_ACONK) | SEJ_AES_BK2C, SEJ_ACONK);
                return 0;
            }

            /* HRK256 / New BK2C */
            sej_aes_kdf(true);

            return 0;
        case AES_HW_WRAP_KEY:
            break;
        case AES_RID_KEY:
            p_key = g_sej_ctx.rid_key;
            break;
        case AES_CUSTOM_KEY:
            p_key = g_sej_ctx.custom_key;
            break;
        default:
            p_key = g_sej_ctx.sw_key;
    }

    writel(readl(SEJ_ACONK) & ~SEJ_AES_BK2C, SEJ_ACONK);
    for (int i=0; i < 8; i++) {
        uint32_t word = cpu_to_be32(p_key[i]);
        writel(word, SEJ_AKEY0 + (i * 4));
    }

    return 0;
}

uint32_t process_desc(sej_param_t desc) {
    uint32_t status = 0;

    /* Set AES mode */
    status = sej_set_mode(desc.mode);

    /* Set Key */
    if (!desc.anti_clone) {
        status = sej_set_key(desc.key_id, desc.key_sz);
    }

    g_sej_ctx.legacy = desc.legacy;

    return status;
}

int sp_sej_enc(uint8_t* buf, uint8_t* out, sej_param_t desc) {
    int success = 0;

    success = process_desc(desc);

    if (success != 0) {
        return success;
    }

    if (!desc.anti_clone) {
        return sej_do_aes(AES_ENC, buf, out, desc.length);
    }

    if (desc.xor_en) {
        uint32_t xor_bytes = (desc.length < 0x10) ? desc.length : 0x10;
        xor_buf(buf, (const uint8_t*)g_AC_BOOTROM_IV, xor_bytes, buf);
    }

    SEJ_V3_init(AES_ENC, (const uint32_t*)g_AC_BOOTROM_IV);
    SEJ_V3_Run((volatile uint32_t*)buf, desc.length, (volatile uint32_t*)out);
    SEJ_V3_Terminate();
    return success;
}

int sp_sej_dec(uint8_t* buf, uint8_t* out, sej_param_t desc) {
    int success = 0;

    success = process_desc(desc);

    if (success != 0) {
        return success;
    }

    if (!desc.anti_clone) {
        return sej_do_aes(AES_DEC, buf, out, desc.length);
    }

    SEJ_V3_init(AES_DEC, (const uint32_t*)g_AC_BOOTROM_IV);
    SEJ_V3_Run((volatile uint32_t*)buf, desc.length, (volatile uint32_t*)out);
    SEJ_V3_Terminate();

    if (desc.xor_en) {
        uint32_t xor_bytes = (desc.length < 0x10) ? desc.length : 0x10;
        xor_buf(buf, (const uint8_t*)g_AC_BOOTROM_IV, xor_bytes, out);
    }

    return success;
}

void sej_get_bootmode(uint32_t* bootmode) {
    *bootmode = readl(SEJ_CON) & 0xF;
}

uint32_t sej_prng(uint32_t* iv, uint32_t* out) {
    /* Wait for SEJ to be idle */
    while(!(readl_relaxed(SEJ_ACON2) & SEJ_AES_RDY));

    /* Switch SEJ from AES to RNG and enable source */
    uint32_t rng_settings = SEJ_RNG_MOD_EN | SEJ_RNG_SRC_EN;

    if (iv) {
        writel(iv[0], SEJ_RNG_IV0);
        writel(iv[1], SEJ_RNG_IV1);
        writel(iv[2], SEJ_RNG_IV2);
        writel(iv[3], SEJ_RNG_IV3);
    }

    /* If IV is provided, tell SEJ */
    rng_settings |= (iv != 0) ? SEJ_RNG_SRC_IV : 0;

    writel(rng_settings, SEJ_RCON);

    /* Start RNG */
    writel(SEJ_RNG_START, SEJ_RCON2);

    while(!(readl_relaxed(SEJ_RCON2) & SEJ_RNG_RDY));

    *out++ = readl(SEJ_RNG_OUT0);
    *out++ = readl(SEJ_RNG_OUT1);
    *out++ = readl(SEJ_RNG_OUT2);
    *out++ = readl(SEJ_RNG_OUT3);

    /* Switch SEJ back to AES */
    while(!(readl_relaxed(SEJ_ACON2) & SEJ_AES_RDY));
    writel(SEJ_RNG_MOD_EN, SEJ_RCON);

    return 0;
}
