#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common.h"

uint32_t *g_boot_mode = (uint32_t *)0x41e44500;
uint32_t *sec_policy = (uint32_t *)0x41e4ee84;

int (*app)() = (void *)(0x41e1ccc8 | 1);

size_t (*dprintf)(const char *format, ...) = (void *)(0x41e20e70 | 1);
size_t (*video_printf)(const char *format, ...) = (void *)(0x41e20c0c | 1);

struct device_t *(*get_device)() = (void *)(0x41e14050 | 1);
int (*sec_set_device_lock)(int do_lock) = (void *)(0x41e280b8 | 1);

int (*fastboot_get_unlock_perm)(unsigned int *unlock_allowed) = (void *)(0x41e1f86c | 1);

void (*fastboot_info)(const char *reason) = (void *)(0x41e1d18c | 1);
void (*fastboot_fail)(const char *reason) = (void *)(0x41e1d1d4 | 1);
void (*fastboot_okay)(const char *reason) = (void *)(0x41e1d37c | 1);
void (*fastboot_register)(const char *prefix,
                                 void (*handle)(const char *arg, void *data,
                                                unsigned sz),
                                 unsigned char security_enabled) = (void *)(0x41e1cf8c | 1);