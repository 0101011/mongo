/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

/*
 * __wt_btree_stat_print --
 *	Print DB handle statistics to a stream.
 */
int
__wt_btree_stat_print(SESSION *session, FILE *stream)
{
	BTREE *btree;
	CONNECTION *conn;

	btree = session->btree;
	conn = btree->conn;

	fprintf(stream, "Btree statistics: %s\n", btree->name);
	__wt_stat_print_btree_stats(btree->stats, stream);
	fprintf(stream, "%s\n", conn->sep);

	/* Clear the file stats, then call Btree stat to fill them in. */
	__wt_stat_clear_btree_file_stats(btree->fstats);
	WT_STAT_SET(
	    btree->fstats, file_freelist_entries, btree->freelist_entries);
	WT_RET(__wt_desc_stat(session));

	/*
	 * Note we do not have a hazard reference for the root page, and that's
	 * safe -- root pages are pinned into memory when a file is opened, and
	 * never re-written until the file is closed.
	 */
	WT_RET(__wt_tree_walk(session, NULL, __wt_page_stat, NULL));

	fprintf(stream, "Btree file statistics: %s\n", btree->name);
	__wt_stat_print_btree_file_stats(btree->fstats, stream);
	fprintf(stream, "%s\n", conn->sep);

	/* Underlying file handle statistics. */
	if (btree->fh != NULL) {
		fprintf(stream, "Btree I/O statistics: %s\n", btree->name);
		__wt_stat_print_file_stats(btree->fh->stats, stream);
		fprintf(stream, "%s\n", conn->sep);
	}

	return (0);
}

/*
 * __wt_btree_stat_clear --
 *	Clear DB handle statistics.
 */
int
__wt_btree_stat_clear(BTREE *btree)
{
	__wt_stat_clear_btree_stats(btree->stats);
	__wt_stat_clear_btree_file_stats(btree->fstats);
	if (btree->fh != NULL)
		__wt_stat_clear_file_stats(btree->fh->stats);

	return (0);
}
