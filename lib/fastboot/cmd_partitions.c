//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/storage.h>

void cmd_partitions(const char* arg, void* data, unsigned sz) {
    int count = storage_part_count();
    if (!count) {
        fastboot_fail("Partition table is not available");
        return;
    }

    char line[FASTBOOT_INFO_MAX + 1];

    fastboot_info("name                    start     blocks");
    for (int i = 0; i < count; i++) {
        const struct part_info* part = storage_part_get(i);
        if (!part) {
            break;
        }

        npf_snprintf(line, sizeof(line), "%-20s %9u %10u", part->name,
                     (unsigned)part->start_block, (unsigned)part->size_blocks);
        fastboot_info(line);
    }

    fastboot_okay("");
}

FASTBOOT_CMD(partitions, "oem partitions", cmd_partitions, 1);
