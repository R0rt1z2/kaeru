// Copyright 2024 (c) B.Kerler
// Copyright 2025 (c) Shomy
// Use of this source code is governed by a GPLv3 license, see LICENSE.txt.

#include <lib/sej.h>
#include <stdint.h>

#define ERR_SEC_CYPHER_KEY_INVALID 0xC0020032
#define ERR_SEC_CYPHER_DATA_UNALIGNED 0xC0020033

uint32_t g_UnqKey_IV[8] = {0x6786CFBD, 0x44B7F1E0, 0x1544B07B, 0x53A28EB3, 0xD7AB8AA2, 0xB9E30E7E, 0x172156E0, 0x3064C973};

sej_ctx_t g_sej_ctx;

const uint32_t G_CFG_RANDOM_PATTERN[12] = {
    0x2D44BB70, 0xA744D227, 0xD0A9864B, 0x83FFC244,
    0x7EC8266B, 0x43E80FB2, 0x01A6348A, 0x2067F9A0,
    0x54536405, 0xD546A6B1, 0x1CC3EC3A, 0xDE377A83
};

const unsigned int g_HACC_CFG_1[8] = {
	0x9ED40400, 0x00E884A1, 0xE3F083BD, 0x2F4E6D8A,
	0xFF838E5C, 0xE940A0E3, 0x8D4DECC6, 0x45FC0989
};

const unsigned int g_HACC_CFG_2[8] = {
	0xAA542CDA, 0x55522114, 0xE3F083BD, 0x55522114,
	0xAA542CDA, 0xAA542CDA, 0x55522114, 0xAA542CDA
};

const unsigned int g_HACC_CFG_3[8] = {
	0x2684B690, 0xEB67A8BE, 0xA113144C, 0x177B1215,
	0x168BEE66, 0x1284B684, 0xDF3BCE3A, 0x217F6FA2
};

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


volatile uint32_t hacc_base=0x1000a000;

int32_t toSigned32(uint32_t n){
    n = n & 0xffffffff;
    return n | (-(n & 0x80000000));
}

static uint32_t get_world_clock_value(void){
    return INREG32(0x10017008);
}

int32_t check_timeout(const uint32_t clockvalue, int32_t timeout){
    uint32_t tmp = -clockvalue;
    const uint32_t curtime = get_world_clock_value();
    if (curtime < clockvalue){
        tmp = ~clockvalue;
    }
    return tmp + get_world_clock_value() >= ((uint32_t)timeout)*1000*13;
}

#define NULL 0

void SEJ_V3_init(bool encrypt, const uint32_t* iv, bool legacy) {
    uint32_t acon_settings =
        SEJ_AES_CHG_BO_OFF |
        SEJ_AES_128 |
        ((iv != 0) ? SEJ_AES_CBC : 0) |
        (encrypt ? SEJ_AES_ENC : SEJ_AES_DEC);

    for (int i = 0; i < 8; i++) {
        OUTREG32(SEJ_AKEY0 + (i * 4), 0);
    }

    OUTREG32(SEJ_ACON,  SEJ_AES_CHG_BO_OFF | SEJ_AES_CBC | SEJ_AES_128 | SEJ_AES_DEC);
    OUTREG32(SEJ_ACONK, SEJ_AES_BK2C | SEJ_AES_R2K);
    OUTREG32(SEJ_ACON2, SEJ_AES_CLR);

    if (iv != 0) {
        OUTREG32(SEJ_ACFG0, iv[0]);
        OUTREG32(SEJ_ACFG1, iv[1]);
        OUTREG32(SEJ_ACFG2, iv[2]);
        OUTREG32(SEJ_ACFG3, iv[3]);
    }

    if (!legacy) {
        /* Non legacy SEJ impl */
        uint32_t val = INREG32(SEJ_UNK) | 2;
        OUTREG32(SEJ_UNK, val);

        val = INREG32(SEJ_ACON2) | 0x40000000;
        OUTREG32(SEJ_ACON2, val);

        while(!(INREG32(SEJ_ACON2) > 0x80000000));

        val = INREG32(SEJ_UNK) & 0xFFFFFFFE;
        OUTREG32(SEJ_UNK, val);
        OUTREG32(SEJ_ACONK, SEJ_AES_BK2C);
        OUTREG32(SEJ_ACON, acon_settings);
    } else {
        /* Legacy impl of SEJ. Used mainly in SECCFG V3 devices */
        // This breaks everything, so commenting out for now
        // OUTREG32(SEJ_UNK, 1);

        // Derives a patterns based from HUID
        for (int i = 0; i < 3; i++) {
            const uint32_t* pattern_block = &G_CFG_RANDOM_PATTERN[i * 4];

            OUTREG32(SEJ_ASRC0, pattern_block[0]);
            OUTREG32(SEJ_ASRC1, pattern_block[1]);
            OUTREG32(SEJ_ASRC2, pattern_block[2]);
            OUTREG32(SEJ_ASRC3, pattern_block[3]);

            OUTREG32(SEJ_ACON2, SEJ_AES_START);
            while(!(INREG32(SEJ_ACON2) & SEJ_AES_RDY));
        }

        OUTREG32(SEJ_ACON2, SEJ_AES_CLR);

        if (iv != 0) {
            OUTREG32(SEJ_ACFG0, iv[0]);
            OUTREG32(SEJ_ACFG1, iv[1]);
            OUTREG32(SEJ_ACFG2, iv[2]);
            OUTREG32(SEJ_ACFG3, iv[3]);
        }

        OUTREG32(SEJ_ACON, acon_settings);
        OUTREG32(SEJ_ACONK, 0);
    }
}

void SEJ_V3_Run(volatile uint32_t* p_src, uint32_t length, volatile uint32_t* p_dst) {
    uint32_t processed_bytes = 0;

    while (processed_bytes < length) {
        OUTREG32(SEJ_ASRC0, p_src[0]);
        OUTREG32(SEJ_ASRC1, p_src[1]);
        OUTREG32(SEJ_ASRC2, p_src[2]);
        OUTREG32(SEJ_ASRC3, p_src[3]);

        OUTREG32(SEJ_ACON2, SEJ_AES_START);

        while(!(INREG32(SEJ_ACON2) & SEJ_AES_RDY));

        p_dst[0] = INREG32(SEJ_AOUT0);
        p_dst[1] = INREG32(SEJ_AOUT1);
        p_dst[2] = INREG32(SEJ_AOUT2);
        p_dst[3] = INREG32(SEJ_AOUT3);

        p_src += 4;
        p_dst += 4;

        processed_bytes += 16;
    }
}

void SEJ_V3_Terminate(void) {
    OUTREG32(SEJ_ACON2,SEJ_AES_CLR);
    for (int32_t i = 0; i < 8; i++){
        OUTREG32(SEJ_AKEY0 + (4 * i),0);
    }
}

int32_t sej_set_otp(uint32_t* otp){
    for (int32_t i = 0; i < 8; i++) {
        OUTREG32(SEJ_SW_OTP0 + (4 * i),otp[i]);
    }
    return 0;
}

int sej_set_key(AES_KEY_ID id,AES_KEY_SZ key) {
    if (0x10 < key - AES_KEY_128) {
        return ERR_SEC_CYPHER_KEY_INVALID;
    }

    uint32_t val = 0;
    uint8_t* p_key = g_sej_ctx.hw_key;
    if (1) {
        switch (id) {
            case AES_HW_KEY:
                p_key = g_sej_ctx.hw_key;
                val = INREG32(SEJ_UNK) | 0x2;
                OUTREG32(SEJ_UNK, val);
                val = INREG32(SEJ_ACON2) | 0x40000000;
                OUTREG32(SEJ_ACON2, val);

                while ((INREG32(SEJ_ACON2) & 1) != 0);

                g_sej_ctx.blk_sz = key;

                val = INREG32(SEJ_ACONK) | 0x10;
                OUTREG32(SEJ_ACONK, val);

                for (int i=0; i < 8; i++)
                    OUTREG32(SEJ_AKEY0 + (i * 4), 0);

                val = INREG32(SEJ_UNK) & 0xFFFFFFFE;
                OUTREG32(SEJ_UNK, val);
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
        for (int i=0; i < 8; i++) {
            // Must be in BE
            uint32_t word = (p_key[i*4] << 24) |
                            (p_key[i*4+1] << 16) |
                            (p_key[i*4+2] << 8) |
                            (p_key[i*4+3]);
            OUTREG32(SEJ_AKEY0 + (i * 4), word);
        }
        return 0;
    }

    return ERR_SEC_CYPHER_KEY_INVALID;
}

int sej_do_aes(AES_OPS ops, uint8_t* src, uint8_t* dst, uint32_t size)
{
    if ((((g_sej_ctx.blk_sz - 1) | 0xF) & size) != 0)
        return ERR_SEC_CYPHER_DATA_UNALIGNED;

    uint32_t val = 0;

    AES_IV* iv = &g_sej_ctx.iv;;
    if (g_sej_ctx.use_custom_iv) {
        iv = &g_sej_ctx.custom_iv;
    }

    uint32_t* iv_words = (uint32_t*)iv->vector;
    OUTREG32(SEJ_ACFG0, iv_words[0]);
    OUTREG32(SEJ_ACFG1, iv_words[1]);
    OUTREG32(SEJ_ACFG2, iv_words[2]);
    OUTREG32(SEJ_ACFG3, iv_words[3]);

    val = (INREG32(SEJ_ACON) & ~1) | (ops & 1);
    OUTREG32(SEJ_ACON, val);

    uint32_t* p_src = (uint32_t*)src;
    uint32_t* p_dst = (uint32_t*)dst;
    uint32_t processed_bytes = 0;

    while (processed_bytes < size) {
        OUTREG32(SEJ_ASRC0, p_src[0]);
        OUTREG32(SEJ_ASRC1, p_src[1]);
        OUTREG32(SEJ_ASRC2, p_src[2]);
        OUTREG32(SEJ_ASRC3, p_src[3]);

        SETREG32(SEJ_ACON2, SEJ_AES_START);

        while (!(INREG32(SEJ_ACON2) & SEJ_AES_RDY));

        p_dst[0] = INREG32(SEJ_AOUT0);
        p_dst[1] = INREG32(SEJ_AOUT1);
        p_dst[2] = INREG32(SEJ_AOUT2);
        p_dst[3] = INREG32(SEJ_AOUT3);

        p_src += 4;
        p_dst += 4;
        processed_bytes += 16;
    }

    return 0;
}

uint32_t sej_set_mode(AES_MODE mode) {
    uint32_t val = 0;
    val = INREG32(SEJ_ACON);
    val &= ~(SEJ_AES_MODE_MASK);
    val |= mode ? SEJ_AES_CBC : 0;
    OUTREG32(SEJ_ACON, val);

    if (mode == AES_ECB_MODE) {
        for (int i = 0; i < 16; i++) {
            g_sej_ctx.iv.vector[i] = 0;
        }
    }

    return 0;
}

uint32_t sej_set_iv(AES_IV* iv) {
    for (int i = 0; i < 16; i++) {
        g_sej_ctx.iv.vector[i] = iv->vector[i];
    }
    return 0;
}

uint32_t sej_set_custom_iv(AES_IV* iv, uint32_t size) {
    if (size > 0x16) {
        return 1;
    }

    for (int i = 0; i < 16; i++) {
        g_sej_ctx.custom_iv.vector[i] = iv->vector[i];
    }
    g_sej_ctx.use_custom_iv = 1;
    return 0;
}

uint32_t sej_set_custom_key(uint8_t* key, uint32_t size) {
    if (size != SEJ_AES_MAX_KEY_SZ) {
        return ERR_SEC_CYPHER_KEY_INVALID;
    }

    for (int i = 0; i < SEJ_AES_MAX_KEY_SZ; i++) {
        g_sej_ctx.custom_key[i] = key[i];
    }

    return 0;
}

int sp_sej_enc(uint8_t* buf, uint8_t* out, uint32_t size, bool anti_clone, bool legacy) {
    int success = 0;
    if (!anti_clone) {
        sej_set_key(AES_SW_KEY, AES_KEY_256);
        success = sej_do_aes(AES_ENC, buf, out, size);
        return success;
    }

    SEJ_V3_init(AES_ENC, (const uint32_t*)g_HACC_CFG_1, legacy);
    SEJ_V3_Run((volatile uint32_t*)buf, size, (volatile uint32_t*)out);
    SEJ_V3_Terminate();
    return success;
}

int sp_sej_dec(uint8_t* buf, uint8_t* out, uint32_t size, bool anti_clone, bool legacy) {
    int success = 0;
    if (!anti_clone) {
        sej_set_key(AES_SW_KEY, AES_KEY_256);
        success = sej_do_aes(AES_DEC, buf, out, size);
        return success;
    }

    SEJ_V3_init(AES_DEC, (const uint32_t*)g_HACC_CFG_1, legacy);
    SEJ_V3_Run((volatile uint32_t*)buf, size, (volatile uint32_t*)out);
    SEJ_V3_Terminate();
    return success;
}

void init_sej_ctx(void) {
    AES_IV iv;

    for (int i = 0; i < 32; i++) {
        g_sej_ctx.sw_key[i] = DEFAULT_KEY[i];
    }

    for (int i = 0; i < 16; i++) {
        iv.vector[i] = DEFAULT_IV[i];
    }

    sej_set_iv(&iv);
    g_sej_ctx.blk_sz = 16;
    g_sej_ctx.use_custom_iv = false;
}

void set_sej_base(uint32_t base_addr) {
    hacc_base = base_addr;
}

uint32_t get_sej_base(void) {
    return hacc_base;
}
