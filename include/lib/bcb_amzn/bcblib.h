// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-or-later
/*
 * bcblib - library for manipulating amazon/mediatek bcb data
 * Copyright (c) 2026 Ben Grisdale <bengris32@protonmail.ch>
 *
 * This work is dual-licensed under the terms of the 3-Clause BSD License
 * or the GNU General Public License (GPL) version 2.0 or later.
 * You may choose, at your discretion, which of the licenses to follow.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/string.h>

#define BCB_MAGIC	"ABB" /* excluding null byte */
#define BCB_MAGIC_SIZE	3
#define BCB_OFFSET	0x360 /* offset into the /misc partition */
#define BCB_MAX_VERSION	1
#define BCB_MAX_SLOTS	2

#define BCB_SLOT_METADATA(_prio, _tries, _success)	\
	(bcb_slot_metadata_t) {				\
	.priority = _prio,				\
	.tries = _tries,				\
	.success = _success,				\
}

/* Empty BCB metadata initializer.
 * Results in slot being unconditionally disabled since all values are 0.
 */
#define BCB_SLOT_METADATA_EMPTY		BCB_SLOT_METADATA(0, 0, 0)

/* Active BCB metadata initializer.
 * Results in the slot being selected since success is set to 1.
 */
#define BCB_SLOT_METADATA_ACTIVE	BCB_SLOT_METADATA(15, 0, 1)

/* Initial BCB metadata initializer.
 * Initializes slot to the default values: 3 attempts to boot; unbootable.
 */
#define BCB_SLOT_METADATA_INIT(prio)	BCB_SLOT_METADATA(prio, 3, 0)

typedef struct {
	uint8_t priority:4;
	uint8_t tries:3;
	uint8_t success:1;
} bcb_slot_metadata_t;

struct bcb {
	uint8_t magic_null; // always 0
	char magic[BCB_MAGIC_SIZE];
	uint8_t version;
	bcb_slot_metadata_t slot[BCB_MAX_SLOTS];
} __attribute__((packed));

/* initialize new BCB metadata with default values.
 * bcb: pointer to bcb structure.
 */
void bcblib_bcb_init(struct bcb *bcb);

/* get BCB slot metadata by name.
 * bcb: pointer to bcb structure.
 * slot: name of slot (i.e. 'a' or 'b')
 */
static inline bcb_slot_metadata_t* bcblib_bcb_get_slot(struct bcb *bcb, unsigned char slot)
{
	unsigned char slot_index = slot - 'a';
	if (slot_index > BCB_MAX_SLOTS)
		return NULL;
	return &bcb->slot[slot_index];
}

/* check if the BCB metadata header is valid
 * bcb: pointer to bcb structure.
 */
static inline bool bcblib_bcb_magic_valid(const struct bcb *bcb)
{
	return bcb->magic_null == 0 &&
	       memcmp(bcb->magic, BCB_MAGIC, BCB_MAGIC_SIZE) == 0 &&
	       bcb->version > 0 && bcb->version <= BCB_MAX_VERSION;
}

/* get active BCB slot. Follows preloader's logic.
 * bcb: pointer to bcb structure.
 * decrement_tries: whether to decrement tries, if being used in a real bootloader.
 * try_other: whether to fallback to the lower priority slot if it has tries > 0.
 */
int bcblib_bcb_get_active_slot(struct bcb *bcb, bool decrement_tries, bool try_other);

/* set priority on BCB slot metadata.
 * metadata: pointer to bcb_slot_metadata_t structure.
 */
static inline void bcblib_metadata_set_priority(bcb_slot_metadata_t *metadata, uint8_t priority)
{
	metadata->priority = priority;
}

/* get priority on BCB slot metadata.
 * metadata: pointer to bcb_slot_metadata_t structure.
 */
static inline uint8_t bcblib_metadata_get_priority(const bcb_slot_metadata_t *metadata)
{
	return metadata->priority;
}

/* set tries on BCB slot metadata.
 * metadata: pointer to bcb_slot_metadata_t structure.
 */
static inline void bcblib_metadata_set_tries(bcb_slot_metadata_t *metadata, uint8_t tries)
{
	metadata->tries = tries;
}

/* get tries on BCB slot metadata.
 * metadata: pointer to bcb_slot_metadata_t structure.
 */
static inline uint8_t bcblib_metadata_get_tries(const bcb_slot_metadata_t *metadata)
{
	return metadata->tries;
}

/* set success on BCB slot metadata.
 * metadata: pointer to bcb_slot_metadata_t structure.
 */
static inline void bcblib_metadata_set_success(bcb_slot_metadata_t *metadata, bool success)
{
	metadata->success = success;
}

/* get success on BCB slot metadata.
 * metadata: pointer to bcb_slot_metadata_t structure.
 */
static inline bool bcblib_metadata_get_success(const bcb_slot_metadata_t *metadata)
{
	return metadata->success;
}
