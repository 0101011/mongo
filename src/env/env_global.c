/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

void *__wt_addr;				/* Memory flush address. */

/*
 * __wt_library_init --
 *	Some things to do, before we do anything else.
 */
int
__wt_library_init(void)
{
	/*
	 * We need an address for memory flushing -- it doesn't matter which
	 * one we choose.
	 */
	__wt_addr = &__wt_addr;

#ifdef HAVE_DIAGNOSTIC
	/* Load debug code the compiler might optimize out. */
	WT_RET(__wt_breakpoint());
#endif

	return (0);
}

/*
 * __wt_breakpoint --
 *	A simple place to put a breakpoint, if you need one.
 */
int
__wt_breakpoint(void)
{
	return (0);
}

int __wt_debugger_attach;

/*
 * __wt_attach --
 *	A routine to wait for the debugging to attach.
 */
void
__wt_attach(SESSION *session)
{
#ifdef HAVE_ATTACH
	__wt_err(session, 0,
	    "process ID %lld: waiting for debugger...", (long long)getpid());
	while (__wt_debugger_attach == 0)
		__wt_sleep(10, 0);
#else
	WT_UNUSED(session);
#endif
}
