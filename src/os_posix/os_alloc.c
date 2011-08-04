/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

/*
 * There's no malloc interface, WiredTiger never calls malloc.  The problem is
 * an application might: allocate memory, write secret stuff into it, free the
 * memory, then we allocate the memory and use it for a file page or log record,
 * and then write it to disk.  That would result in the secret stuff being
 * protected by the WiredTiger permission mechanisms, potentially inappropriate
 * for the secret stuff.
 */

/*
 * __wt_calloc --
 *	ANSI calloc function.
 */
int
__wt_calloc(WT_SESSION_IMPL *session, size_t number, size_t size, void *retp)
{
	void *p;

	/*
	 * !!!
	 * This function MUST handle a NULL WT_SESSION_IMPL handle.
	 */
	WT_ASSERT(session, number != 0 && size != 0);

	if (session != NULL && S2C(session)->stats != NULL)
		WT_CSTAT_INCR(session, memalloc);

	if ((p = calloc(number, (size_t)size)) == NULL) {
		__wt_err(session, errno, "memory allocation");
		return (WT_ERROR);
	}

	*(void **)retp = p;
	return (0);
}

/*
 * __wt_realloc --
 *	ANSI realloc function.
 */
int
__wt_realloc(WT_SESSION_IMPL *session,
    uint32_t *bytes_allocated_ret, size_t bytes_to_allocate, void *retp)
{
	void *p;
	size_t bytes_allocated;

	/*
	 * !!!
	 * This function MUST handle a NULL WT_SESSION_IMPL handle.
	 */
	WT_ASSERT(session, bytes_to_allocate != 0);

	if (session != NULL && S2C(session)->stats != NULL)
		WT_CSTAT_INCR(session, memalloc);

	p = *(void **)retp;

	/*
	 * Sometimes we're allocating memory and we don't care about the
	 * final length -- bytes_allocated_ret may be NULL.
	 */
	bytes_allocated =
	    bytes_allocated_ret == NULL ? 0 : *bytes_allocated_ret;
	WT_ASSERT(session, bytes_allocated < bytes_to_allocate);

	if ((p = realloc(p, bytes_to_allocate)) == NULL) {
		__wt_err(session, errno, "memory allocation");
		return (WT_ERROR);
	}

	/*
	 * Clear the allocated memory -- an application might: allocate memory,
	 * write secret stuff into it, free the memory, then we re-allocate the
	 * memory and use it for a file page or log record, and then write it to
	 * disk.  That would result in the secret stuff being protected by the
	 * WiredTiger permission mechanisms, potentially inappropriate for the
	 * secret stuff.
	 */
	memset((uint8_t *)
	    p + bytes_allocated, 0, bytes_to_allocate - bytes_allocated);

	/* Update caller's bytes allocated value. */
	if (bytes_allocated_ret != NULL) {
		WT_ASSERT(session,
		    bytes_to_allocate == (uint32_t)bytes_to_allocate);
		*bytes_allocated_ret = (uint32_t)bytes_to_allocate;
	}

	*(void **)retp = p;
	return (0);
}

/*
 * __wt_strndup --
 *	Duplicate a string of a given length (and NUL-terminate).
 */
int
__wt_strndup(WT_SESSION_IMPL *session, const char *str, size_t len, void *retp)
{
	void *p;

	if (str == NULL) {
		*(void **)retp = NULL;
		return (0);
	}

	/*
	 * !!!
	 * This function MUST handle a NULL WT_SESSION_IMPL handle.
	 */
	if (session != NULL && S2C(session)->stats != NULL)
		WT_CSTAT_INCR(session, memalloc);

	WT_RET(__wt_calloc(session, len + 1, 1, &p));
	memcpy(p, str, len);

	*(void **)retp = p;
	return (0);
}

/*
 * __wt_strdup --
 *	ANSI strdup function.
 */
int
__wt_strdup(WT_SESSION_IMPL *session, const char *str, void *retp)
{
	return (__wt_strndup(session,
	    str, (str == NULL) ? 0 : strlen(str), retp));
}

/*
 * __wt_free_int --
 *	ANSI free function.
 */
void
__wt_free_int(WT_SESSION_IMPL *session, void *p_arg)
{
	void *p;

	/*
	 * !!!
	 * This function MUST handle a NULL WT_SESSION_IMPL handle.
	 */
	if (session != NULL && S2C(session)->stats != NULL)
		WT_CSTAT_INCR(session, memfree);

	/*
	 * If there's a serialization bug we might race with another thread.
	 * We can't avoid the race (and we aren't willing to flush memory),
	 * but we minimize the window by clearing the free address atomically,
	 * hoping a racing thread will see, and won't free, a NULL pointer.
	 */
	p = *(void **)p_arg;
	*(void **)p_arg = NULL;

	if (p != NULL)			/* ANSI C free semantics */
		free(p);
}
