/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

/*
 * __wt_sb_alloc --
 *	Allocate memory from the WT_SESSION_IMPL's buffer and fill it in.
 */
int
__wt_sb_alloc(
    WT_SESSION_IMPL *session, size_t size, void *retp, WT_SESSION_BUFFER **sbp)
{
#ifndef HAVE_SESSION_BUFFERS
	*sbp = NULL;
	return (__wt_calloc(session, 1, size, retp));
#else
	WT_SESSION_BUFFER *sb;
	size_t alloc_size;
	uint32_t align_size;
	int single_use;

	/*
	 * Allocate memory for an insert or change; there's a buffer in the
	 * WT_SESSION_IMPL structure for allocation of chunks of memory to hold
	 * changed or inserted values.
	 *
	 * We align allocations because we directly access WT_UPDATE structure
	 * fields in the memory (the x86 handles unaligned accesses, but I don't
	 * want to have to find and fix this code for a port to a system that
	 * doesn't handle unaligned accesses).	It wastes space, but this memory
	 * is never written to disk and there are fewer concerns about memory
	 * than with on-disk structures.  Any other code allocating memory from
	 * this buffer needs to align its allocations as well.
	 *
	 * The first thing in each chunk of memory is a WT_SESSION_BUFFER
	 * structure (check to be a multiple of 4B during initialization);
	 * then one or more WT_UPDATE structure plus value chunk pairs.
	 *
	 * Figure out how much space we need: this code limits the maximum size
	 * of a data item stored in the file.  In summary, for a big item we
	 * have to store a WT_SESSION_BUFFER structure, the WT_UPDATE structure
	 * and the data, all in an allocated buffer.   We only pass a 32-bit
	 * value to our allocation routine, so we can't store an item bigger
	 * than the maximum 32-bit value minus the sizes of those two
	 * structures, where the WT_UPDATE structure and data item are aligned
	 * to a 32-bit boundary.  We could fix this, but it's unclear it's
	 * worth the effort: document you can store a (4GB - 512B) item max,
	 * it's insane to store 4GB items in the file anyway.
	 */
	if (size > WT_BTREE_OBJECT_SIZE_MAX)
		return (__wt_file_item_too_big(session));
	align_size = WT_ALIGN(size + sizeof(WT_UPDATE), sizeof(uint32_t));

	/* If we already have a buffer and the data fits, we're done. */
	sb = session->sb;
	if (sb != NULL && align_size <= sb->space_avail)
		goto no_allocation;

	/*
	 * We start by allocating 4KB for the thread, then every time we have
	 * to re-allocate the buffer, we double the allocation size, up to a
	 * total of 8MB, so any thread doing a lot of updates won't re-allocate
	 * new chunks of memory that often.
	 */
	if (session->update_alloc_size == 0) {
		/*
		 * 2KB is correct, we're going to double it to 4KB when we
		 * calculate a new allocation size.
		 */
		session->update_alloc_size = 2 * 1024;

		/*
		 * We don't want to never aggregate changes because records are
		 * initially relatively large, compared to the allocation size,
		 * that is, if the application is loading many 4KB records, we'd
		 * like to handle that reasonably.  This code adjusts for that
		 * case.
		 *
		 * If we get nothing but 256KB inserts, this code will allocate
		 * each of them individually, without aggregation, never growing
		 * the aggregation buffer size.  That doesn't seem all that bad,
		 * aggregation isn't intended for lots of large records, rather
		 * it's intended for lots of small records.
		 */
		if (align_size > session->update_alloc_size &&
		    align_size < 128 * 1024)
			session->update_alloc_size = 128 * 1024;
	}

	/*
	 * Decide how much memory to allocate: if it's a one-off (that is, the
	 * value is bigger than anything we'll aggregate into these buffers),
	 * allocate just enough memory.  Else, allocate the next power-of-two
	 * larger, up to 8MB.
	 */
	if (align_size > session->update_alloc_size) {
		alloc_size = sizeof(WT_SESSION_BUFFER) + align_size;
		single_use = 1;
	} else {
		if (session->update_alloc_size < 8 * WT_MEGABYTE)
			session->update_alloc_size =
			    __wt_nlpo2(session->update_alloc_size);
		alloc_size = session->update_alloc_size;
		single_use = 0;
	}

	WT_RET(__wt_calloc(session, 1, alloc_size, &sb));
	sb->len = WT_STORE_SIZE(alloc_size);
	sb->space_avail = WT_STORE_SIZE(alloc_size - sizeof(WT_SESSION_BUFFER));
	sb->first_free = (uint8_t *)sb + sizeof(WT_SESSION_BUFFER);

	/*
	 * If it's a single use allocation, ignore any current buffer in the
	 * session; else, release the old session buffer and replace it with
	 * the new one.
	 */
	if (!single_use) {
		/*
		 * The "in" reference count is artificially incremented by 1 as
		 * long as a session buffer is referenced by the session
		 * handle; we do not want session buffers freed because a page
		 * was evicted and the count went to 0 while the buffer might
		 * still be used for future K/V inserts or modifications.
		 */
		if (session->sb != NULL)
			__wt_sb_decrement(session, session->sb);
		session->sb = sb;

		sb->in = 1;
	}

no_allocation:
	*(void **)retp = sb->first_free;
	*sbp = sb;

	sb->first_free += align_size;
	sb->space_avail -= align_size;
	++sb->in;
	WT_ASSERT(session, sb->in != 0);

	return (0);
#endif
}

/*
 * __wt_sb_free --
 *	Free a chunk of memory from a per-WT_SESSION_IMPL buffer.
 */
void
__wt_sb_free(WT_SESSION_IMPL *session, WT_SESSION_BUFFER *sb, void *p)
{
#ifndef HAVE_SESSION_BUFFERS
	WT_UNUSED(sb);

	__wt_free(session, p);
#else
	WT_ASSERT(session, sb->out < sb->in);

	if (++sb->out == sb->in)
		__wt_free(session, sb);
#endif
}

/*
 * __wt_sb_decrement --
 *	Decrement the "insert" value of a per-WT_SESSION_IMPL buffer.
 */
void
__wt_sb_decrement(WT_SESSION_IMPL *session, WT_SESSION_BUFFER *sb, void *p)
{
#ifndef HAVE_SESSION_BUFFERS
	WT_UNUSED(sb);

	__wt_free(session, p);
#else
	WT_ASSERT(session, sb->out < sb->in);

	/*
	 * This function is used for two reasons.
	 *
	 * #1: it's possible we allocated memory from the session buffer, but
	 * then an error occurred.  In this case we don't try and clean up the
	 * session buffer, it's simpler to decrement the counters and pretend
	 * the memory is no longer in use.  We're still in the allocation path
	 * so we decrement the "in" field instead of incrementing the "out"
	 * field, if the eviction thread were to update the "out" field at the
	 * same time, we could race.
	 *
	 * #2: the "in" reference count is artificially incremented by 1 as
	 * long as a session buffer is referenced by the session handle; we do
	 * not want session buffers freed because a page was evicted and the
	 * count went to 0 while the buffer might still be used for future K/V
	 * inserts or modifications.
	 */
	--sb->in;

	/*
	 * In the above case #1, if the session buffer was a one-off (allocated
	 * for a single use), we have to free it here, it's not linked to any
	 * WT_PAGE in the system.
	 *
	 * In the above case #2, our artificial increment might be the last
	 * reference, if all of the WT_PAGE's referencing this buffer have been
	 * reconciled since the K/V inserts or modifications.
	 *
	 * In both of these cases, sb->in == sb->out, and we need to free the
	 * buffer.
	 *
	 * XXX
	 * There's a race here in the above case #2: if this code, and the page
	 * discard code race, it's possible neither will realize the buffer is
	 * no longer needed and free it.  The fix is to involve the eviction
	 * thread: it may need a linked list of buffers they review to ensure
	 * it never happens.  I'm living with this now: it's an unlikely
	 * race, and it's a memory leak if it ever happens.
	 */
	if (sb->in == sb->out)
		__wt_free(session, sb);
#endif
}
