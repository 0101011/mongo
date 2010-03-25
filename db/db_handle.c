/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008 WiredTiger Software.
 *	All rights reserved.
 *
 * $Id$
 */

#include "wt_internal.h"

static int  __wt_db_config_default(DB *);
static int  __wt_idb_config_default(DB *);

/*
 * __wt_env_db --
 *	DB constructor.
 */
int
__wt_env_db(ENV *env, DB **dbp)
{
	DB *db;
	IDB *idb;
	IENV *ienv;
	int ret;

	db = NULL;
	idb = NULL;
	ienv = env->ienv;

	/* Create the DB and IDB structures. */
	WT_ERR(__wt_calloc(env, 1, sizeof(DB), &db));
	WT_ERR(__wt_calloc(env, 1, sizeof(IDB), &idb));

	/* Connect everything together. */
	db->idb = idb;
	idb->db = db;
	db->env = env;

	/* Configure the DB and the IDB. */
	WT_ERR(__wt_db_config_default(db));
	WT_ERR(__wt_idb_config_default(db));

	*dbp = db;
	return (0);

err:	(void)__wt_db_close(db);
	return (ret);
}

/*
 * __wt_db_config_default --
 *	Set default configuration for a just-created DB handle.
 */
static int
__wt_db_config_default(DB *db)
{
	ENV *env;
	IDB *idb;

	env = db->env;
	idb = db->idb;

	__wt_methods_db_lockout(db);
	__wt_methods_db_init_transition(db);

	db->btree_compare = db->btree_compare_dup = __wt_bt_lex_compare;

	WT_RET(__wt_stat_alloc_db_stats(env, &idb->stats));
	WT_RET(__wt_stat_alloc_database_stats(env, &idb->dstats));

	return (0);
}

/*
 * __wt_idb_config_default --
 *	Set default configuration for a just-created IDB handle.
 */
static int
__wt_idb_config_default(DB *db)
{
	ENV *env;
	IDB *idb;
	IENV *ienv;

	env = db->env;
	idb = db->idb;
	ienv = env->ienv;

	idb->db = db;

	__wt_lock(env, ienv->mtx);		/* Add to the ENV's list */
	TAILQ_INSERT_TAIL(&ienv->dbqh, idb, q);
	__wt_unlock(ienv->mtx);

	return (0);
}

/*
 * __wt_db_close --
 *	Db.close method (DB close & handle destructor).
 */
int
__wt_db_close(DB *db)
{
	ENV *env;
	int ret;

	env = db->env;
	ret = 0;

	/* Close the underlying Btree. */
	WT_TRET(__wt_bt_close(db));

	/* Discard the underlying IDB object. */
	WT_TRET(__wt_idb_close(db, 0));

	/* Discard the DB object. */
	__wt_free(env, db, sizeof(DB));

	return (ret);
}

/*
 * __wt_idb_close --
 *	Db.close method (IDB close & handle destructor).
 */
int
__wt_idb_close(DB *db, int refresh)
{
	ENV *env;
	IDB *idb;
	IENV *ienv;
	int ret;

	env = db->env;
	idb = db->idb;
	ienv = env->ienv;
	ret = 0;

	/* Check that there's something to close. */
	if (idb == NULL)
		return (0);

	/* Diagnostic check: check flags against approved list. */
	WT_ENV_FCHK_RET(env, "Db.close", idb->flags, WT_APIMASK_IDB, ret);

	if (idb->huffman_key != NULL) {
		/* Key and data may use the same table, only close it once. */
		if (idb->huffman_data == idb->huffman_key)
			idb->huffman_data = NULL;
		__wt_huffman_close(env, idb->huffman_key);
		idb->huffman_key = NULL;
	}
	if (idb->huffman_data != NULL) {
		__wt_huffman_close(env, idb->huffman_data);
		idb->huffman_data = NULL;
	}

	/* Free any allocated memory. */
	__wt_free(env, idb->dbname, 0);
	__wt_free(env, idb->stats, 0);
	__wt_free(env, idb->dstats, 0);

	/*
	 * This is the guts of the split between the public/private, DB/IDB
	 * handles.  If a Db.open fails for any reason, the user may use the
	 * DB structure again, but the IDB structure may have been modified
	 * in the attempt.  So, we overwrite the IDB structure, as if it was
	 * just allocated.  This requires the IDB structure never be modified
	 * by DB configuration, we'd lose that configuration here.
	 */
	if (refresh) {
		memset(idb, 0, sizeof(idb));
		WT_TRET(__wt_idb_config_default(db));
		return (ret);
	}

	__wt_lock(env, ienv->mtx);		/* Delete from the ENV's list */
	TAILQ_REMOVE(&ienv->dbqh, idb, q);
	__wt_unlock(ienv->mtx);

	__wt_free(env, idb, sizeof(IDB));

	db->idb = NULL;
	return (0);
}

int
__wt_db_lockout_err(DB *db)
{
	__wt_api_db_errx(db,
	    "This Db handle has failed for some reason, and can no longer "
	    "be used; the only method permitted on it is Db.close which "
	    "discards the handle permanently");
	return (WT_ERROR);
}

int
__wt_db_lockout_open(DB *db)
{
	__wt_api_db_errx(db,
	    "This method may not be called until after the Db.open method has "
	    "been called");
	return (WT_ERROR);
}
