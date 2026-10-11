//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

void oem_table_add_command(const char* prefix,
                           void (*handle)(const char* arg, void* data, unsigned sz));
