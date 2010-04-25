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
 * __wt_bt_build_verify --
 *	Verify the Btree build itself.
 */
int
__wt_bt_build_verify(void)
{
	static const struct {
		u_int s, c, align;
		char *name;
	} list[] = {
		{ sizeof(WT_BIN_INDX), WT_BIN_INDX_SIZE, 0, "WT_BIN_INDX" },
		{ sizeof(WT_COL_INDX), WT_COL_INDX_SIZE, 0, "WT_COL_INDX" },
		{ sizeof(WT_ITEM), WT_ITEM_SIZE, 0, "WT_ITEM" },
		{ sizeof(WT_OFF), WT_OFF_SIZE, sizeof(u_int32_t), "WT_OFF" },
		{ sizeof(WT_OVFL), WT_OVFL_SIZE, sizeof(u_int32_t), "WT_OVFL" },
		{ sizeof(WT_PAGE), WT_PAGE_SIZE, 0, "WT_PAGE" },
		{ sizeof(WT_PAGE_DESC), WT_PAGE_DESC_SIZE, 0, "WT_PAGE_DESC" },
		{ sizeof(WT_PAGE_HDR),
		    WT_PAGE_HDR_SIZE, sizeof(u_int32_t), "WT_PAGE_HDR" },
		{ sizeof(WT_ROW_INDX), WT_ROW_INDX_SIZE, 0, "WT_ROW_INDX" }
	}, *lp;

	/*
	 * The compiler had better not have padded our structures -- make
	 * sure the page header structure is exactly what we expect.
	 */
	for (lp = list; lp < list + sizeof(list) / sizeof(list[0]); ++lp) {
		if (lp->s == lp->c)
			continue;
		__wt_api_env_errx(NULL,
		    "WiredTiger build failed, the %s header structure is not "
		    "the correct size (expected %u, got %u)",
		    lp->name, lp->c, lp->s);
		return (WT_ERROR);
	}

	/* There are also structures that must be aligned correctly. */
	for (lp = list; lp < list + sizeof(list) / sizeof(list[0]); ++lp) {
		if (lp->align == 0 || WT_ALIGN(lp->s, lp->align) == lp->s)
			continue;
		__wt_api_env_errx(NULL,
		    "Build verification failed, the %s structure is not"
		    " correctly aligned", lp->name);
		return (WT_ERROR);
	}

	/*
	 * We mix-and-match 32-bit unsigned values and size_t's, mostly because
	 * we allocate and handle 32-bit objects, and lots of the underlying C
	 * library expects size_t values for the length of memory objects.  We
	 * check, just to be sure.
	 */
	if (sizeof(size_t) < sizeof(u_int32_t)) {
		__wt_api_env_errx(NULL, "%s",
		    "Build verification failed, a size_t is smaller than "
		    "4-bytes");
		return (WT_ERROR);
	}

	return (0);
}

/*
 * __wt_bt_data_copy_to_dbt --
 *	Copy a data/length pair into allocated memory in a DBT.
 */
int
__wt_bt_data_copy_to_dbt(DB *db, u_int8_t *data, size_t len, DBT *copy)
{
	ENV *env;

	env = db->env;

	if (copy->data == NULL || copy->mem_size < len)
		WT_RET(__wt_realloc(env, &copy->mem_size, len, &copy->data));
	memcpy(copy->data, data, copy->size = len);

	return (0);
}

/*
 * __wt_bt_set_ff_and_sa_from_p --
 *	Set the page's first-free and space-available values from an
 *	address positioned one past the last used byte on the page.
 */
void
__wt_bt_set_ff_and_sa_from_addr(WT_PAGE *page, u_int8_t *p)
{
	page->first_free = p;
	page->space_avail = page->size - (u_int)(p - (u_int8_t *)page->hdr);
}

/*
 * __wt_bt_hdr_type --
 *	Return a string representing the page type.
 */
const char *
__wt_bt_hdr_type(WT_PAGE_HDR *hdr)
{
	switch (hdr->type) {
	case WT_PAGE_DESCRIPT:
		return ("database descriptor page");
	case WT_PAGE_COL_FIX:
		return ("column fixed-length leaf");
	case WT_PAGE_COL_INT:
		return ("column internal");
	case WT_PAGE_COL_VAR:
		return ("column variable-length leaf");
	case WT_PAGE_DUP_INT:
		return ("duplicate internal");
	case WT_PAGE_DUP_LEAF:
		return ("duplicate leaf");
	case WT_PAGE_OVFL:
		return ("overflow");
	case WT_PAGE_ROW_INT:
		return ("row internal");
	case WT_PAGE_ROW_LEAF:
		return ("row leaf");
	case WT_PAGE_INVALID:
		return ("invalid");
	default:
		break;
	}
	return ("unknown");
}

/*
 * __wt_bt_item_type --
 *	Return a string representing the item type.
 */
const char *
__wt_bt_item_type(WT_ITEM *item)
{
	switch (WT_ITEM_TYPE(item)) {
	case WT_ITEM_DATA:
		return ("data");
	case WT_ITEM_DATA_OVFL:
		return ("data-overflow");
	case WT_ITEM_DUP:
		return ("duplicate");
	case WT_ITEM_DUP_OVFL:
		return ("duplicate-overflow");
	case WT_ITEM_KEY:
		return ("key");
	case WT_ITEM_KEY_OVFL:
		return ("key-overflow");
	case WT_ITEM_OFF:
		return ("off-page");
	default:
		break;
	}
	return ("unknown");
}
