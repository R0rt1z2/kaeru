//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025-2026 Shomy
//

#ifndef XOR_H
#define XOR_H

#include <stdint.h>

void xor_buf(const uint8_t* buf, const uint8_t* key, uint32_t size, uint8_t* out);

#endif // XOR_H
