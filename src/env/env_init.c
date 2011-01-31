/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 *
 * $Id$
 */

#include "wt_internal.h"

/*
 * wiredtiger_env_init --
 *	Initialize the library, creating an ENV handle.
 */
int
wiredtiger_env_init(ENV **envp, uint32_t flags)
{
	static int library_init = 0;
	ENV *env;

	*envp = NULL;

	/*
	 * We end up here before we do any real work.   Check the build itself,
	 * and do some global stuff.
	 */
	if (library_init == 0) {
		WT_RET(__wt_library_init());
		library_init = 1;
	}

	WT_ENV_FCHK(NULL,
	    "wiredtiger_env_init", flags, WT_APIMASK_WIREDTIGER_ENV_INIT);

	/* Create the ENV handle. */
	WT_RET(__wt_env_create(flags, &env));

	*envp = env;
	return (0);
}
