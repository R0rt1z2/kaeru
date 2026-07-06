//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025-2026 Shomy
//

#include <lib/crypto/xor.h>

void xor_buf(const uint8_t* buf, const uint8_t* key, uint32_t size, uint8_t* out) {
    for (uint32_t i = 0; i < size; i++) {
        out[i] = buf[i] ^ key[i];
    }
}
