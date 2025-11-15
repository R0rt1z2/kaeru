//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/thread.h>

thread_t *thread_create(const char *name, thread_start_routine entry, void *arg, int priority, size_t stack_size) {
    return ((thread_t *(*)(const char *, thread_start_routine, void *, int, size_t))
            (CONFIG_THREAD_CREATE_ADDRESS | 1))(name, entry, arg, priority, stack_size);
}

int thread_resume(thread_t *t) {
    return ((int (*)(thread_t *))
            (CONFIG_THREAD_RESUME_ADDRESS | 1))(t);
}
