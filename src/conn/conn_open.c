/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

/*
 * __wt_connection_open --
 *	Open a connection.
 */
int
__wt_connection_open(WT_CONNECTION_IMPL *conn, const char *cfg[])
{
	WT_SESSION_IMPL *session;
	int ret;

	/* Default session. */
	conn->default_session.iface.connection = &conn->iface;

	session = &conn->default_session;
	ret = 0;

	/* WT_SESSION_IMPL and hazard arrays. */
	WT_ERR(__wt_calloc(session,
	    conn->session_size, sizeof(WT_SESSION_IMPL *), &conn->sessions));
	WT_ERR(__wt_calloc(session,
	    conn->session_size, sizeof(WT_SESSION_IMPL),
	    &conn->session_array));
	WT_ERR(__wt_calloc(session,
	   conn->session_size * conn->hazard_size, sizeof(WT_HAZARD),
	   &conn->hazard));

	/* Create the cache. */
	WT_ERR(__wt_cache_create(conn, cfg));

	/*
	 * Publish: there must be a barrier to ensure the connection structure
	 * fields are set before other threads read from the pointer.
	 */
	WT_WRITE_BARRIER();

	/* Start worker threads. */
	F_SET(conn, WT_SERVER_RUN);
	WT_ERR(__wt_thread_create(
	    &conn->cache_evict_tid, __wt_cache_evict_server, conn));

	return (0);

err:	(void)__wt_connection_close(conn);
	return (ret);
}

/*
 * __wt_connection_close --
 *	Close a connection handle.
 */
int
__wt_connection_close(WT_CONNECTION_IMPL *conn)
{
	WT_BTREE *btree;
	WT_SESSION_IMPL *session;
	WT_DLH *dlh;
	WT_FH *fh;
	int ret;

	session = &conn->default_session;
	ret = 0;

	/* Complain if WT_BTREE handles weren't closed. */
	while ((btree = TAILQ_FIRST(&conn->btqh)) != NULL) {
		if (F_ISSET(btree, WT_BTREE_OPEN))
			__wt_errx(session,
			    "Connection has an open btree handle: %s",
			    btree->name);
		WT_SET_BTREE_IN_SESSION(session, btree);

		/*
		 * XXX
		 * This won't work, you can't close a btree handle with the
		 * default session.
		 */
		WT_TRET(__wt_btree_close(session));
	}

	/*
	 * Complain if files weren't closed (ignoring the lock and logging
	 * files, we'll close them in a minute.
	 */
	TAILQ_FOREACH(fh, &conn->fhqh, q) {
		if (fh == conn->lock_fh || fh == conn->log_fh)
			continue;

		__wt_errx(session,
		    "Connection has open file handles: %s", fh->name);
		WT_TRET(__wt_close(session, fh));
		fh = TAILQ_FIRST(&conn->fhqh);
	}

	/* Shut down the server threads. */
	F_CLR(conn, WT_SERVER_RUN);
	if (conn->cache_evict_tid != 0) {
		__wt_evict_server_wake(session);
		WT_TRET(__wt_thread_join(conn->cache_evict_tid));
	}

	/* Discard the cache. */
	__wt_cache_destroy(conn);

	/* Close extensions. */
	while ((dlh = TAILQ_FIRST(&conn->dlhqh)) != NULL) {
		TAILQ_REMOVE(&conn->dlhqh, dlh, q);
		WT_TRET(__wt_dlclose(session, dlh));
	}

	/* Destroy the handle. */
	__wt_connection_destroy(conn);

	return (ret);
}
