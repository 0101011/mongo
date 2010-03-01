/*
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2009 WiredTiger Software.
 *	All rights reserved.
 *
 * $Id$
 */

#include <stdlib.h>

#include "wiredtiger.h"

extern const char *progname;

static ENV *__env;

/*
 * wiredtiger_simple_setup --
 *	Standard setup for simple applications.
 */
int
wiredtiger_simple_setup(
    const char *progname, DB **dbp, u_int32_t cache_size, u_int32_t flags)
{
	DB *db;
	ENV *env;
	int ret;

	db = *dbp = NULL;

	if ((ret = wiredtiger_env_init(&env, flags)) != 0) {
		fprintf(stderr,
		    "%s: wiredtiger_env_init: %s\n",
		    progname, wiredtiger_strerror(ret));
		return (ret);
	}
	__env = env;

	if (cache_size != 0 &&
	    (ret = env->cache_size_set(env, cache_size)) != 0) {
		env->err(env, ret, "Env.cache_size_set");
		goto err;
	}

	if ((ret = env->open(env, NULL, 0, 0)) != 0) {
		env->err(env, ret, "%s: Env.open", progname);
		goto err;
	}
	if ((ret = env->db(env, 0, &db)) != 0) {
		env->err(env, ret, "%s: Env.db", progname);
err:		wiredtiger_simple_teardown(progname, db);
		return (ret);
	}

	*dbp = db;
	return (EXIT_SUCCESS);
}

/*
 * wiredtiger_simple_teardown --
 *	Standard teardown for simple applications.
 */
int
wiredtiger_simple_teardown(const char *progname, DB *db)
{
	int ret, tret;

	ret = 0;
	if (db != NULL && (tret = db->close(db, 0)) != 0) {
		fprintf(stderr,
		    "%s: Db.close: %s\n", progname, wiredtiger_strerror(ret));
		if (ret == 0)
			ret = tret;
	}

	if (__env != NULL) {
		if ((tret = __env->close(__env, 0)) != 0) {
			fprintf(stderr, "%s: Env.close: %s\n",
			    progname, wiredtiger_strerror(ret));
			if (ret == 0)
				ret = tret;
		}
		__env = NULL;
	}

	return (ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
