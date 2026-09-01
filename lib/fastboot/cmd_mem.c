//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/string.h>

#define MEM_READ_DEFAULT 0x100
#define MEM_READ_MAX     0x400

static void mem_hexdump(uint32_t addr, uint32_t len) {
    const uint8_t* p = (const uint8_t*)(uintptr_t)addr;
    char line[FASTBOOT_INFO_MAX + 1];

    for (uint32_t off = 0; off < len; off += 8) {
        uint32_t n = (len - off) > 8 ? 8 : (len - off);
        size_t used = 0;
        uint32_t i;

        used += npf_snprintf(line + used, sizeof(line) - used, "%08X ",
                             (unsigned)(addr + off));

        for (i = 0; i < n; i++) {
            used += npf_snprintf(line + used, sizeof(line) - used, " %02x", p[off + i]);
        }

        for (; i < 8; i++) {
            used += npf_snprintf(line + used, sizeof(line) - used, "   ");
        }

        used += npf_snprintf(line + used, sizeof(line) - used, "  |");

        for (i = 0; i < n; i++) {
            uint8_t c = p[off + i];

            used += npf_snprintf(line + used, sizeof(line) - used, "%c",
                                 (c >= 0x20 && c < 0x7F) ? c : '.');
        }

        npf_snprintf(line + used, sizeof(line) - used, "|");
        fastboot_info(line);
    }
}

void cmd_mem(const char* arg, void* data, unsigned sz) {
    char op[8];
    char tok[40];
    char line[FASTBOOT_INFO_MAX + 1];

    const char* p = next_token(arg, op, sizeof(op));

    p = next_token(p, tok, sizeof(tok));
    if (!tok[0]) {
        fastboot_fail("Usage: oem mem read|write <addr> [len|value]");
        return;
    }

    const char* end;
    uint32_t addr = (uint32_t)parse_hex64(tok, &end);
    uint32_t region_size;

    if (*end && !(mem_region_find && mem_region_find(tok, &addr, &region_size))) {
        fastboot_fail(mem_region_find ? "Address is neither a number nor a known region"
                                      : "Address is not a number");
        return;
    }

    p = next_token(p, tok, sizeof(tok));

    if (strcmp(op, "read") == 0) {
        uint32_t len = MEM_READ_DEFAULT;

        if (tok[0]) {
            len = (uint32_t)parse_hex64(tok, NULL);
        }

        if (!len || len > MEM_READ_MAX) {
            npf_snprintf(line, sizeof(line), "Length must be 1 to 0x%X", MEM_READ_MAX);
            fastboot_fail(line);
            return;
        }

        mem_hexdump(addr, len);
        fastboot_okay("");
        return;
    }

    if (strcmp(op, "write") == 0) {
        if (!tok[0]) {
            fastboot_fail("Usage: oem mem write <addr> <value>");
            return;
        }

        if (addr & 3) {
            fastboot_fail("Address must be word aligned");
            return;
        }

        uint32_t value = (uint32_t)parse_hex64(tok, NULL);

        WRITE32(addr, value);

        npf_snprintf(line, sizeof(line), "%08X = %08X", (unsigned)addr,
                     (unsigned)READ32(addr));
        fastboot_info(line);
        fastboot_okay("");
        return;
    }

    fastboot_fail("Usage: oem mem read|write <addr> [len|value]");
}

FASTBOOT_CMD(mem, "oem mem", cmd_mem, 1);
