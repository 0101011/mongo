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
 * __wt_db_row_get --
 *	Db.row_get method.
 */
int
__wt_db_row_get(WT_TOC *toc, DBT *key, DBT *data)
{
	DB *db;
	IDB *idb;
	WT_PAGE *page;
	WT_ROW_INDX *ip;
	u_int32_t type;
	int ret;

	db = toc->db;
	idb = db->idb;
	page = NULL;

	/* Search the btree for the key. */
	WT_ERR(__wt_bt_search_row(toc, key, 0));
	page = toc->srch_page;
	ip = toc->srch_ip;

	/*
	 * The Db.get method can only return single key/data pairs.
	 * If that's not what we found, we're done.
	 *
	 * XXX
	 * Checking if page_data is NULL isn't the right thing to do
	 * here.   Re-visit this when we figure out how we handle
	 * dup inserts into the tree.
	 */
	if (ip->data != NULL) {
		type = WT_ITEM_TYPE(ip->data);
		if (type != WT_ITEM_DATA && type != WT_ITEM_DATA_OVFL) {
			__wt_api_db_errx(db,
			    "the Db.get method cannot return keys with "
			    "duplicate data items; use the Db.cursor method "
			    "instead");
			ret = WT_ERROR;
			goto err;
		}
	}
	ret = __wt_bt_dbt_return(toc, key, data, page, ip, 0);

err:	if (page != NULL && page != idb->root_page)
		__wt_bt_page_out(toc, &page, 0);
	return (ret);
}
