/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

/*
 * __wt_page_type_string --
 *	Return a string representing the page type.
 */
const char *
__wt_page_type_string(u_int type)
{
	switch (type) {
	case WT_PAGE_INVALID:
		return ("invalid");
	case WT_PAGE_COL_FIX:
		return ("column-store fixed-length leaf");
	case WT_PAGE_COL_INT:
		return ("column-store internal");
	case WT_PAGE_COL_VAR:
		return ("column-store variable-length leaf");
	case WT_PAGE_OVFL:
		return ("overflow");
	case WT_PAGE_ROW_INT:
		return ("row-store internal");
	case WT_PAGE_ROW_LEAF:
		return ("row-store leaf");
	case WT_PAGE_FREELIST:
		return ("freelist");
	default:
		return ("unknown");
	}
	/* NOTREACHED */
}

/*
 * __wt_cell_type_string --
 *	Return a string representing the cell type.
 */
const char *
__wt_cell_type_string(uint8_t type)
{
	switch (type) {
	case WT_CELL_ADDR:
		return ("address");
	case WT_CELL_DEL:
		return ("deleted");
	case WT_CELL_KEY:
		return ("key");
	case WT_CELL_KEY_OVFL:
		return ("key-overflow");
	case WT_CELL_KEY_SHORT:
		return ("short-key");
	case WT_CELL_VALUE:
		return ("value");
	case WT_CELL_VALUE_OVFL:
		return ("value-overflow");
	case WT_CELL_VALUE_SHORT:
		return ("short-value");
	default:
		return ("illegal");
	}
	/* NOTREACHED */
}

/*
 * __wt_page_addr_string --
 *	Figure out a page's "address" and load a buffer with a printable,
 * nul-terminated representation of that address.
 */
const char *
__wt_page_addr_string(WT_SESSION_IMPL *session, WT_BUF *buf, WT_PAGE *page)
{
	WT_BTREE *btree;
	uint32_t size;
	const uint8_t *addr;

	btree = session->btree;

	if (WT_PAGE_IS_ROOT(page)) {
		addr = btree->root_addr;
		size = btree->root_size;
	} else
		__wt_get_addr(page->parent, page->parent_ref.ref, &addr, &size);

	return (__wt_addr_string(session, buf, addr, size));
}

/*
 * __wt_addr_string --
 *	Load a buffer with a printable, nul-terminated representation of an
 * address.
 */
const char *
__wt_addr_string(
    WT_SESSION_IMPL *session, WT_BUF *buf, const uint8_t *addr, uint32_t size)
{
	if (addr == NULL) {
		buf->data = "[NoAddr]";
		buf->size = WT_STORE_SIZE(strlen("[NoAddr]"));
	} else if (__wt_bm_addr_string(session, buf, addr, size) != 0) {
		buf->data = "[Error]";
		buf->size = WT_STORE_SIZE(strlen("[Error]"));
	}
	return ((char *)buf->data);
}
