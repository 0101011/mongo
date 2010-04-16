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
 * __wt_db_col_get --
 *	Db.col_get method.
 */
int
__wt_db_col_get(WT_TOC *toc, u_int64_t recno, DBT *data)
{
	DB *db;
	IDB *idb;
	WT_PAGE *page;
	int ret;

	db = toc->db;
	idb = db->idb;
	page = NULL;

	/* Search the column store for the key. */
	if (!F_ISSET(idb, WT_COLUMN)) {
		__wt_api_db_errx(db,
		    "row database records cannot be retrieved by record "
		    "number");
		return (WT_ERROR);
	}

	WT_ERR(__wt_bt_search_col(toc, recno));
	page = toc->srch_page;
	ret = __wt_bt_dbt_return(toc, NULL, data, page, toc->srch_ip, 0);

err:	if (page != NULL && page != idb->root_page)
		__wt_bt_page_out(toc, &page, 0);
	return (ret);
}
