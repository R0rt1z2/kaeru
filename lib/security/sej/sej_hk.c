/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2025-2026 Shomy
 */

#include <arch/mmio.h>
#include <lib/security/sej.h>

#define SEJ_KDF_SPIN_MAX 0x100000

void sej_aes_kdf(bool hrk256) {
    uint32_t aconk2_settings = 0;

    /* Enable or disable HRK */
    aconk2_settings = readl(SEJ_ACONK2);
    aconk2_settings &= ~SEJ_HRK_EN;
    aconk2_settings |= hrk256 ? SEJ_HRK_EN : SEJ_HRK_DIS;

    writel(aconk2_settings, SEJ_ACONK2);

    /* Start KDF */
    writel(readl(SEJ_ACON2) | SEJ_AES_KDF_START, SEJ_ACON2);

    for (uint32_t spins = 0; !(readl_relaxed(SEJ_ACON2) & SEJ_KDF_RDY); spins++) {
        if (spins >= SEJ_KDF_SPIN_MAX)
            return;
    }

    /* Clear Legacy BK2C in ACONK2 and bind new BK2C */
    writel(readl(SEJ_ACONK2) & ~SEJ_AES_OLD_BK2C, SEJ_ACONK2);
    writel(readl(SEJ_ACONK) | SEJ_AES_BK2C, SEJ_ACONK);
}

void sej_aes_old_bk2c_key(void) {
    uint32_t aconk2_settings = readl(SEJ_ACONK2);

    /* Clear new BK2C */
    aconk2_settings &= ~SEJ_HRK_EN;
    /* Bind legacy BK2C */
    aconk2_settings |= SEJ_AES_OLD_BK2C;

    writel(aconk2_settings, SEJ_ACONK2);
    writel(SEJ_AES_BK2C, SEJ_ACONK);
}

void SEJ_V3_init(AES_OPS encrypt, const uint32_t* iv) {
    uint32_t acon_settings = 0;

    /* Keep byte order to LE */
    acon_settings = SEJ_AES_CHG_BO_OFF;

    /* Perform AES with 128 bit key */
    acon_settings |= SEJ_AES_128;

    /* If an IV is provided, use CBC mode, otherwise use ECB mode */
    acon_settings |= ((iv != 0) ? SEJ_AES_CBC : SEJ_AES_ECB);

    /* Set encryption/decryption mode */
    acon_settings |= encrypt & 0xF;

    /* Clear key regs */
    for (int i = 0; i < 8; i++) {
        writel(0, SEJ_AKEY0 + (i * 4));
    }

    /* Generate Meta Key (Master key?) */
    writel(SEJ_AES_CHG_BO_OFF | SEJ_AES_CBC | SEJ_AES_128 | SEJ_AES_DEC, SEJ_ACON);
    /* Bind Hardware Unique Key (BK2C) and R2K to SEJ */
    writel(SEJ_AES_BK2C | SEJ_AES_R2K, SEJ_ACONK);
    writel(SEJ_AES_CLR, SEJ_ACON2);

    if (iv != 0) {
        writel(iv[0], SEJ_ACFG0);
        writel(iv[1], SEJ_ACFG1);
        writel(iv[2], SEJ_ACFG2);
        writel(iv[3], SEJ_ACFG3);
    }


    if (!g_sej_ctx.legacy) {
        sej_aes_kdf(true);
        /* The original implementation just inlines
         * the KDF and directly assigns BK2C on ACONK.
         * However, I just called `sej_aes_kdf` here for
         * avoiding repetition.
         *
         * However, the original implementation assigns
         * BK2C only to ACONK, so this makes sure that
         * R2K is not used during AES operation, which
         * causes the data to have the feedback of SEJ
         * internal algorithm.
         */
        writel(readl(SEJ_ACONK) & ~SEJ_AES_R2K, SEJ_ACONK);
    } else {
        /* Derive a pattern HUK */
        for (uint32_t i = 0; i < 3; i++) {
            writel(g_AC_FIXED_PATTERN[i][0], SEJ_ASRC0);
            writel(g_AC_FIXED_PATTERN[i][1], SEJ_ASRC1);
            writel(g_AC_FIXED_PATTERN[i][2], SEJ_ASRC2);
            writel(g_AC_FIXED_PATTERN[i][3], SEJ_ASRC3);

            writel(SEJ_AES_START, SEJ_ACON2);
            while(!(readl_relaxed(SEJ_ACON2) & SEJ_AES_RDY));
        }

        /* Clear AES_SRC, AES_CFG and AES_OUT */
        writel(SEJ_AES_CLR, SEJ_ACON2);

        if (iv != 0) {
            writel(iv[0], SEJ_ACFG0);
            writel(iv[1], SEJ_ACFG1);
            writel(iv[2], SEJ_ACFG2);
            writel(iv[3], SEJ_ACFG3);
        }

        /* Unbind HUK */
        writel(0, SEJ_ACONK);
    }

    writel(acon_settings, SEJ_ACON);
}

void SEJ_V3_Run(volatile uint32_t* p_src, uint32_t length, volatile uint32_t* p_dst) {
    uint32_t processed_bytes = 0;

    while (processed_bytes < length) {
        writel(p_src[0], SEJ_ASRC0);
        writel(p_src[1], SEJ_ASRC1);
        writel(p_src[2], SEJ_ASRC2);
        writel(p_src[3], SEJ_ASRC3);

        writel(SEJ_AES_START, SEJ_ACON2);

        while(!(readl_relaxed(SEJ_ACON2) & SEJ_AES_RDY));

        p_dst[0] = readl(SEJ_AOUT0);
        p_dst[1] = readl(SEJ_AOUT1);
        p_dst[2] = readl(SEJ_AOUT2);
        p_dst[3] = readl(SEJ_AOUT3);

        p_src += 4;
        p_dst += 4;

        processed_bytes += 16;
    }
}

void SEJ_V3_Terminate(void) {
    writel(SEJ_AES_CLR, SEJ_ACON2);
    for (int32_t i = 0; i < 8; i++){
        writel(0, SEJ_AKEY0 + (4 * i));
    }
}
