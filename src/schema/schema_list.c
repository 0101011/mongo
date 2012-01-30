/*-
 * Copyright (c) 2008-2012 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_schema_add_table --
 *	Add a btree handle to the session's cache.
 */
int
__wt_schema_add_table(
    WT_SESSION_IMPL *session, WT_TABLE *table)
{
	TAILQ_INSERT_HEAD(&session->tables, table, q);

	return (0);
}

/*
 * __wt_schema_get_table --
 *	Get the btree handle for the named table.
 */
int
__wt_schema_find_table(WT_SESSION_IMPL *session,
    const char *name, size_t namelen, WT_TABLE **tablep)
{
	WT_TABLE *table;
	const char *tablename;

	TAILQ_FOREACH(table, &session->tables, q) {
		tablename = table->name;
		WT_PREFIX_SKIP(tablename, "table:");
		if (strncmp(tablename, name, namelen) == 0 &&
		    tablename[namelen] == '\0') {
			*tablep = table;
			return (0);
		}
	}

	return (WT_NOTFOUND);
}

/*
 * __wt_schema_get_table --
 *	Get the btree handle for the named table.
 */
int
__wt_schema_get_table(WT_SESSION_IMPL *session,
    const char *name, size_t namelen, WT_TABLE **tablep)
{
	int ret;

	ret = __wt_schema_find_table(session, name, namelen, tablep);

	if (ret == WT_NOTFOUND) {
		WT_RET(__wt_schema_open_table(session, name, namelen, tablep));
		ret = __wt_schema_add_table(session, *tablep);
	}

	return (ret);
}

/*
 * __wt_schema_remove_table --
 *	Remove the btree handle from the session, closing if necessary.
 */
int
__wt_schema_remove_table(
    WT_SESSION_IMPL *session, WT_TABLE *table)
{
	TAILQ_REMOVE(&session->tables, table, q);
	__wt_free(session, table->name);
	__wt_free(session, table->config);
	__wt_free(session, table->plan);
	__wt_free(session, table->key_format);
	__wt_free(session, table->value_format);
	__wt_free(session, table->colgroup);
	__wt_free(session, table->index);
	__wt_free(session, table);

	return (0);
}

/*
 * __wt_schema_close_tables --
 *	Close all of the tables in a session.
 */
int
__wt_schema_close_tables(WT_SESSION_IMPL *session)
{
	WT_TABLE *table;
	int ret;

	ret = 0;
	while ((table = TAILQ_FIRST(&session->tables)) != NULL)
		WT_TRET(__wt_schema_remove_table(session, table));

	return (ret);
}
