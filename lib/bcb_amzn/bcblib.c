// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-or-later
/*
 * bcblib - library for manipulating amazon/mediatek bcb data
 * Copyright (c) 2026 Ben Grisdale <bengris32@protonmail.ch>
 *
 * This work is dual-licensed under the terms of the 3-Clause BSD License
 * or the GNU General Public License (GPL) version 2.0 or later.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <lib/bcb_amzn/bcblib.h>

/* initialize new BCB metadata with default values.
 * bcb: pointer to bcb structure.
 */
void bcblib_bcb_init(struct bcb *bcb)
{
	bcb->magic_null = 0;
	memcpy(bcb->magic, BCB_MAGIC, BCB_MAGIC_SIZE);
	bcb->version = BCB_MAX_VERSION;

	/* default values */
	bcb->slot[0] = BCB_SLOT_METADATA_INIT(15);
	bcb->slot[1] = BCB_SLOT_METADATA_INIT(14);
}

/* get active BCB slot. Follows preloader's logic.
 * bcb: pointer to bcb structure.
 */
int bcblib_bcb_get_active_slot(struct bcb *bcb,
			       bool decrement_tries,
			       bool try_other)
{
	int slot_index = 0, other_index = 1;

	/* select slot with highest priority */
	if (bcblib_metadata_get_priority(&bcb->slot[1]) >
	    bcblib_metadata_get_priority(&bcb->slot[0])) {
		slot_index = 1;
		other_index = 0;
	}

	/* if tries is > 0 or priority slot is already bootable, we are done */
	if (bcblib_metadata_get_success(&bcb->slot[slot_index]))
		return slot_index;

	if (bcblib_metadata_get_tries(&bcb->slot[slot_index]) > 0) {
		/* try this slot */
		if (decrement_tries) bcb->slot[slot_index].tries--;
		return slot_index;
	}

	/* the priority slot is unbootable. see if the other slot
	 * is bootable.
	 */
	if (bcblib_metadata_get_success(&bcb->slot[other_index]))
		return other_index;

	/* try the other slot if allowed. */
	if (try_other && (bcblib_metadata_get_tries(&bcb->slot[other_index]) > 0)) {
		if (decrement_tries) bcb->slot[other_index].tries--;
		return other_index;
	}

	/* give up */
	return -1;
}
