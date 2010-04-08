/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008 WiredTiger Software.
 *	All rights reserved.
 *
 * $Id$
 */

#include "wt_internal.h"

/*
 * __wt_db_sync --
 *	Flush a database to the backing file.
 */
int
__wt_db_sync(WT_TOC *toc, void (*f)(const char *, u_int64_t))
{
	return (__wt_bt_sync(toc, f));
}
