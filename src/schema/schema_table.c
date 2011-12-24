/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

static const char *schematab_config = "key_format=S,value_format=S";

/*
 * __open_schema_table --
 *	Opens the schema table, sets session->schematab.
 */
static inline int
__open_schema_table(WT_SESSION_IMPL *session)
{
	const char *cfg[] = API_CONF_DEFAULTS(file, meta, schematab_config);
	const char *schemaconf;
	int ret;

	ret = 0;

	if (session->schematab != NULL)
		return (0);

	WT_RET(__wt_config_collapse(session, cfg, &schemaconf));
	WT_ERR(__wt_schema_create(session,
	    "file:" WT_SCHEMA_FILENAME, schemaconf));
	session->schematab = session->btree;
err:	__wt_free(session, schemaconf);
	return (ret);
}

/*
 * __wt_schema_table_cursor --
 *	Opens a cursor on the schema table.
 */
int
__wt_schema_table_cursor(
    WT_SESSION_IMPL *session, const char *config, WT_CURSOR **cursorp)
{
	const char *cfg[] = API_CONF_DEFAULTS(session, open_cursor, config);

	WT_RET(__open_schema_table(session));
	session->btree = session->schematab;
	return (__wt_curfile_create(session, cfg, cursorp));
}

/*
 * __wt_schema_table_insert --
 *	Insert a row into the schema table.
 */
int
__wt_schema_table_insert(
    WT_SESSION_IMPL *session, const char *key, const char *value)
{
	WT_CURSOR *cursor;
	int ret;

	ret = 0;

	WT_RET(__wt_schema_table_cursor(session, NULL, &cursor));
	cursor->set_key(cursor, key);
	cursor->set_value(cursor, value);
	WT_TRET(cursor->insert(cursor));
	WT_TRET(cursor->close(cursor, NULL));
	return (ret);
}

/*
 * __wt_schema_table_update --
 *	Update a row in the schema table.
 */
int
__wt_schema_table_update(
    WT_SESSION_IMPL *session, const char *key, const char *value)
{
	WT_CURSOR *cursor;
	int ret;

	ret = 0;

	WT_RET(__wt_schema_table_cursor(session, "overwrite", &cursor));
	cursor->set_key(cursor, key);
	cursor->set_value(cursor, value);
	WT_TRET(cursor->insert(cursor));
	WT_TRET(cursor->close(cursor, NULL));
	return (ret);
}

/*
 * __wt_schema_table_remove --
 *	Removes a row from the schema table.
 */
int
__wt_schema_table_remove(WT_SESSION_IMPL *session, const char *key)
{
	WT_CURSOR *cursor;
	int ret;

	ret = 0;

	WT_RET(__wt_schema_table_cursor(session, NULL, &cursor));
	cursor->set_key(cursor, key);
	WT_TRET(cursor->remove(cursor));
	WT_TRET(cursor->close(cursor, NULL));
	return (ret);
}

/*
 * __wt_schema_table_read --
 *	Reads and copies a row from the schema table.
 *	The caller is responsible for freeing the allocated memory.
 */
int
__wt_schema_table_read(
    WT_SESSION_IMPL *session, const char *key, const char **valuep)
{
	WT_CURSOR *cursor;
	const char *value;
	int ret;

	ret = 0;

	WT_RET(__wt_schema_table_cursor(session, NULL, &cursor));
	cursor->set_key(cursor, key);
	WT_ERR(cursor->search(cursor));
	WT_ERR(cursor->get_value(cursor, &value));
	WT_ERR(__wt_strdup(session, value, valuep));

err:    WT_TRET(cursor->close(cursor, NULL));
	return (ret);
}
