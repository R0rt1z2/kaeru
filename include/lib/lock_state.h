//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

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
