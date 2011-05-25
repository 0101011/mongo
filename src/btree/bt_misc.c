/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"
#include "cell.i"

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
	case WT_PAGE_COL_RLE:
		return ("column-store fixed-length run-length encoded leaf");
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
		break;
	}
	return ("unknown");
}

/*
 * __wt_cell_type_string --
 *	Return a string representing the cell type.
 */
const char *
__wt_cell_type_string(WT_CELL *cell)
{
	switch (__wt_cell_type_raw(cell)) {
	case WT_CELL_DATA:
		return ("data");
	case WT_CELL_DATA_OVFL:
		return ("data-overflow");
	case WT_CELL_DATA_SHORT:
		return ("data-short");
	case WT_CELL_DEL:
		return ("deleted");
	case WT_CELL_KEY:
		return ("key");
	case WT_CELL_KEY_OVFL:
		return ("key-overflow");
	case WT_CELL_KEY_SHORT:
		return ("key-short");
	case WT_CELL_OFF:
		return ("off-page");
	default:
		return ("illegal");
	}
	/* NOTREACHED */
}
