/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

/*
 * __wt_ovfl_in --
 *	Read an overflow item from the disk.
 */
int
__wt_ovfl_in(WT_SESSION_IMPL *session, WT_OFF *ovfl, WT_BUF *store)
{
	WT_BTREE *btree;
	WT_CONNECTION_IMPL *conn;

	conn = S2C(session);
	btree = session->btree;

	/*
	 * Read an overflow page, using an overflow structure from a page for
	 * which we (better) have a hazard reference.
	 *
	 * Overflow reads are synchronous. That may bite me at some point, but
	 * WiredTiger supports large page sizes, and overflow items should be
	 * rare.
	 */
	WT_VERBOSE(session, WT_VERB_READ, (session,
	    "overflow read addr/size %" PRIu32 "/%" PRIu32,
	    ovfl->addr, ovfl->size));
	WT_STAT_INCR(btree->stats, overflow_read);
	WT_STAT_INCR(conn->cache->stats, cache_overflow_read);

	/*
	 * The only caller that wants a copy of the overflow pages (as opposed
	 * to the contents of the overflow pages), is the verify code.  For that
	 * reason, it reads its own overflow pages, it doesn't call this code.
	 *
	 * But, we still have to verify the checksum, which means we have to
	 * read the entire set of pages, then copy the interesting information
	 * to the beginning of the buffer.   The copy is a shift in a single
	 * buffer and so should be fast, but it's still not a good thing.  If
	 * it ever becomes a problem, then we either have to pass the fact that
	 * it's a "page" back to our caller and let them deal with the offset,
	 * or add a new field to the WT_ITEM that flags the start of the
	 * allocated buffer, instead of using the "data" field to indicate both
	 * the start of the data and the start of the allocated memory.
	 *
	 * Re-allocate memory as necessary to hold the overflow pages.
	 */
	WT_RET(__wt_buf_initsize(session, store, ovfl->size));

	/* Read the page. */
	WT_RET(__wt_disk_read(session, store->mem, ovfl->addr, ovfl->size));

	/* Reference the start of the data and set the data's length. */
	store->data = WT_PAGE_DISK_BYTE(store->mem);
	store->size = ((WT_PAGE_DISK *)store->mem)->u.datalen;

	return (0);
}
