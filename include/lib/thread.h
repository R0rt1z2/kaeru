//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stdint.h>
#include <stddef.h>

#include <lib/list.h>

struct arch_thread {
	uintptr_t sp;
};

typedef int (*thread_start_routine)(void *arg);

enum thread_state {
	THREAD_SUSPENDED = 0,
	THREAD_READY,
	THREAD_RUNNING,
	THREAD_BLOCKED,
	THREAD_SLEEPING,
	THREAD_DEATH,
};

enum thread_tls_list {
	MAX_TLS_ENTRY
};

#define NUM_PRIORITIES 32
#define LOWEST_PRIORITY 0
#define HIGHEST_PRIORITY (NUM_PRIORITIES - 1)
#define DPC_PRIORITY HIGHEST_PRIORITY  // (NUM_PRIORITIES - 2)
#define IDLE_PRIORITY LOWEST_PRIORITY
#define LOW_PRIORITY (NUM_PRIORITIES / 4)
#define DEFAULT_PRIORITY (NUM_PRIORITIES / 2)
#define HIGH_PRIORITY ((NUM_PRIORITIES / 4) * 3)

#define DEFAULT_STACK_SIZE 8192

#define THREAD_MAGIC 'thrd'

typedef struct thread {
	int magic;
	struct list_node thread_list_node;
	struct list_node queue_node;
	int priority;
	enum thread_state state;	
	int saved_critical_section_count;
	int remaining_quantum;
	struct wait_queue *blocking_wait_queue;
	int wait_queue_block_ret;
	struct arch_thread arch;
	void *stack;
	size_t stack_size;
	thread_start_routine entry;
	void *arg;
	int retcode;
	uint32_t tls[MAX_TLS_ENTRY];
	char name[32];
} thread_t;

thread_t *thread_create(const char *name, thread_start_routine entry, void *arg, int priority, size_t stack_size);
int thread_resume(thread_t *);
