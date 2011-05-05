/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"
#include "btree.i"

/*
 * __wt_key_build --
 *	Instantiate an overflow or compressed key into a WT_ROW structure.
 */
int
__wt_key_build(SESSION *session, WT_PAGE *page, void *rip_arg, WT_BUF *store)
{
	WT_BUF scratch;
	const WT_CELL *cell;
	WT_ROW *rip;
	uint32_t size;
	int ret;
	void *key;

	ret = 0;

	/*
	 * Passed both WT_ROW_REF and WT_ROW structures; the first two fields
	 * of the structures are a void *data/uint32_t size pair.
	 */
	rip = rip_arg;
	cell = rip->key;

	/*
	 * Multiple threads of control may be searching this page, which means
	 * we have to serialize instantiating this key, and here's where it
	 * gets tricky.  A few instructions ago we noted the key size was 0,
	 * which meant the key required processing, and we just copied the key.
	 * If another thread instantiated the key while we were doing that,
	 * then the key may have already been instantiated, otherwise, we still
	 * need to proceed.
	 *
	 * We don't want the serialization function to call malloc, which means
	 * we want to instantiate the key here, and only call the serialization
	 * function to swap the key into place.  Check the pointer -- if it's
	 * off-page, we have a key that can be processed, regardless of what any
	 * other thread is doing.
	 *
	 * If it's on-page, we raced and we're done -- create the local copy for
	 * our caller, if that's what they wanted.
	 */
	if (__wt_ref_off_page(page, cell)) {
		if (store == NULL)
			return (0);
		return (__wt_buf_set(session, store, rip->key, rip->size));
	}

	/*
	 * If our user passes us a temporary buffer for storage, don't install
	 * the key in the in-memory page, our caller just needs a local copy.
	 */
	if (store != NULL)
		return (__wt_cell_process(session, cell, store));

	/*
	 * Instantiate a copy of the key in a temporary buffer, then steal the
	 * buffer.
	 */
	WT_CLEAR(scratch);
	WT_RET(__wt_cell_process(session, cell, &scratch));
	__wt_buf_steal(session, &scratch, &key, &size);

	/* Serialize the swap of the key into place. */
	__wt_key_build_serial(session, rip_arg, key, size, ret);

	/* Free allocated memory if the workQ didn't use it for the key. */
	if (rip->key != key)
		__wt_buf_free(session, key);

	return (ret);
}

/*
 * __wt_key_build_serial_func --
 *	Server function to instantiate a key during a row-store search.
 */
int
__wt_key_build_serial_func(SESSION *session)
{
	WT_ROW *rip;
	uint32_t size;
	void *key;

	__wt_key_build_unpack(session, rip, key, size);

	/*
	 * We don't care about the page's write generation -- there's a simpler
	 * test, if the key we're interested in still needs to be instantiated,
	 * because it can only be in one of two states.
	 *
	 * Passed both WT_ROW_REF and WT_ROW structures; the first two fields
	 * of the structures are a void *data/uint32_t size pair.
	 */
	if (__wt_key_process(rip)) {
		/*
		 * Update the key, flush memory, and then update the size.  Done
		 * in that order so any other thread is guaranteed to either see
		 * a size of 0 (indicating the key needs processing, which means
		 * we'll resolve it all here), or see a non-zero size and valid
		 * pointer pair.
		 */
		rip->key = key;
		WT_MEMORY_FLUSH;
		rip->size = size;
	}

	__wt_session_serialize_wrapup(session, NULL, 0);
	return (0);
}
