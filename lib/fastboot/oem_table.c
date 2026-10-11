//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/arm.h>
#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/oem_table.h>
#include <lib/string.h>

#define MAX_COMMANDS 16
#define CMD_SCRATCH_SIZE 1024
#define ARG_MAX 256

struct cmd_table_entry {
    uint32_t flag0;
    uint32_t flag1;
    uint32_t flag2;
    const char* name;
    void* handle;
};

static struct {
    const char* name;
    void (*handle)(const char* arg, void* data, unsigned sz);
} commands[MAX_COMMANDS];
static int command_count;

static uint8_t cmd_scratch[CMD_SCRATCH_SIZE] __attribute__((aligned(4)));

static int table_append(uint32_t head_addr, uint32_t tail_addr, uint8_t* scratch,
                        uint32_t cap, int* relocated, const void* entry, uint32_t size) {
    if (!head_addr || !tail_addr)
        return 0;

    volatile uint32_t* head = (volatile uint32_t*)head_addr;
    volatile uint32_t* tail = (volatile uint32_t*)tail_addr;

    if (!*relocated) {
        uint32_t old_size = *tail - *head;
        if (old_size + size > cap)
            return 0;
        memcpy(scratch, (void*)*head, old_size);
        *head = (uint32_t)scratch;
        *tail = (uint32_t)scratch + old_size;
        *relocated = 1;
        arch_clean_invalidate_cache_range((uintptr_t)head, sizeof(uint32_t));
    }

    uint32_t used = *tail - (uint32_t)scratch;
    if (used + size > cap)
        return 0;

    memcpy(scratch + used, entry, size);
    *tail = (uint32_t)scratch + used + size;

    arch_clean_invalidate_cache_range((uintptr_t)tail, sizeof(uint32_t));
    arch_clean_invalidate_cache_range((uintptr_t)(scratch + used), size);
    return 1;
}

static int cmd_trampoline(int argc, const char** argv) {
    const char* name = (argc > 0) ? argv[0] : "";

    void (*handle)(const char*, void*, unsigned) = NULL;
    for (int i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            handle = commands[i].handle;
            break;
        }
    }
    if (!handle) {
        fastboot_fail("Unknown command");
        return -1;
    }

    static char arg[ARG_MAX];
    unsigned n = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1 && n < sizeof(arg) - 1)
            arg[n++] = ' ';
        for (const char* p = argv[i]; *p && n < sizeof(arg) - 1; p++)
            arg[n++] = *p;
    }
    arg[n] = '\0';

    handle(arg, NULL, 0);
    return 0;
}

void oem_table_add_command(const char* prefix,
                           void (*handle)(const char* arg, void* data, unsigned sz)) {
    if (command_count >= MAX_COMMANDS)
        return;

    const char* name = prefix;
    if (strncmp(name, "oem ", 4) == 0)
        name += 4;

    commands[command_count].name = name;
    commands[command_count].handle = handle;

    struct cmd_table_entry entry = {
            .flag0 = 0,
            .flag1 = 1,
            .flag2 = 0,
            .name = name,
            .handle = (void*)((uintptr_t)cmd_trampoline | 1),
    };

    static int relocated;
    if (table_append(CONFIG_FASTBOOT_OEM_CMD_HEAD, CONFIG_FASTBOOT_OEM_CMD_TAIL, cmd_scratch,
                     CMD_SCRATCH_SIZE, &relocated, &entry, sizeof(entry)))
        command_count++;
    else
        printf("Failed to register OEM command '%s'\n", name);
}
