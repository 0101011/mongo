/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"
#include "btree.i"
#include "cell.i"

#ifdef HAVE_DIAGNOSTIC
static int  __wt_debug_cell(SESSION *, WT_CELL *, FILE *fp);
static int  __wt_debug_cell_data(SESSION *, const WT_CELL *, FILE *);
static void __wt_debug_col_insert(WT_INSERT *, FILE *);
static int  __wt_debug_dsk_cell(SESSION *, WT_PAGE_DISK *, FILE *);
static void __wt_debug_dsk_col_fix(BTREE *, WT_PAGE_DISK *, FILE *);
static void __wt_debug_dsk_col_int(WT_PAGE_DISK *, FILE *);
static void __wt_debug_dsk_col_rle(BTREE *, WT_PAGE_DISK *, FILE *);
static void __wt_debug_page_col_fix(SESSION *, WT_PAGE *, FILE *);
static int  __wt_debug_page_col_int(SESSION *, WT_PAGE *, FILE *, uint32_t);
static void __wt_debug_page_col_rle(SESSION *, WT_PAGE *, FILE *);
static int  __wt_debug_page_col_var(SESSION *, WT_PAGE *, FILE *);
static int  __wt_debug_page_row_int(SESSION *, WT_PAGE *, FILE *, uint32_t);
static int  __wt_debug_page_row_leaf(SESSION *, WT_PAGE *, FILE *);
static int  __wt_debug_page_work(SESSION *, WT_PAGE *, FILE *, uint32_t);
static void __wt_debug_pair(const char *, const void *, uint32_t, FILE *);
static void __wt_debug_ref(WT_REF *, FILE *);
static void __wt_debug_row_insert(WT_INSERT *, FILE *);
static int  __wt_debug_set_fp(const char *, FILE **, int *);
static void __wt_debug_update(WT_UPDATE *, FILE *);

/*
 * __wt_debug_set_fp --
 *	Set the output stream for debugging messages.
 */
static int
__wt_debug_set_fp(const char *ofile, FILE **fpp, int *close_varp)
{
	FILE *fp;

	*close_varp = 0;

	/* If we were giving a stream, use it. */
	if ((fp = *fpp) != NULL)
		return (0);

	/* If we were given a file, use it. */
	if (ofile != NULL) {
		if ((fp = fopen(ofile, "w")) == NULL)
			return (WT_ERROR);
		*fpp = fp;
		*close_varp = 1;
		return (0);
	}

	/* Default to stdout. */
	*fpp = stdout;
	return (0);
}

/*
 * __wt_debug_dump --
 *	Dump a file in debugging mode.  The btree handle should already be
 *	open.
 */
int
__wt_debug_dump(SESSION *session, const char *ofile, FILE *fp)
{
	int do_close, ret;

	WT_RET(__wt_debug_set_fp(ofile, &fp, &do_close));

	/*
	 * We use the verification code to do debugging dumps because if we're
	 * dumping in debugging mode, we want to confirm the page is OK before
	 * walking it.
	 */
	ret = __wt_verify(session, fp, NULL);

	if (do_close)
		(void)fclose(fp);

	return (ret);
}

/*
 * __wt_debug_addr --
 *	Read and dump a disk page in debugging mode.
 */
int
__wt_debug_addr(
    SESSION *session, uint32_t addr, uint32_t size, const char *ofile, FILE *fp)
{
	int do_close, ret;
	char *bp;

	ret = 0;

	WT_RET(__wt_debug_set_fp(ofile, &fp, &do_close));

	WT_RET(__wt_calloc_def(session, (size_t)size, &bp));

	WT_ERR(__wt_disk_read(session, (WT_PAGE_DISK *)bp, addr, size));
	ret = __wt_debug_disk(session, (WT_PAGE_DISK *)bp, NULL, fp);

err:	__wt_free(session, bp);

	if (do_close)
		(void)fclose(fp);

	return (ret);
}

/*
 * __wt_debug_disk --
 *	Dump a disk page in debugging mode.
 */
int
__wt_debug_disk(
    SESSION *session, WT_PAGE_DISK *dsk, const char *ofile, FILE *fp)
{
	BTREE *btree;
	int do_close, ret;

	btree = session->btree;
	ret = 0;

	WT_RET(__wt_debug_set_fp(ofile, &fp, &do_close));

	switch (dsk->type) {
	case WT_PAGE_COL_FIX:
	case WT_PAGE_COL_INT:
	case WT_PAGE_COL_RLE:
	case WT_PAGE_COL_VAR:
		fprintf(fp,
		    "%s page: starting recno %llu, entries %lu, lsn %lu/%lu\n",
		    __wt_page_type_string(dsk->type),
		    (unsigned long long)dsk->recno, (u_long)dsk->u.entries,
		    (u_long)WT_LSN_FILE(dsk->lsn),
		    (u_long)WT_LSN_OFFSET(dsk->lsn));
		break;
	case WT_PAGE_ROW_INT:
	case WT_PAGE_ROW_LEAF:
		fprintf(fp,
		    "%s page: entries %lu, lsn %lu/%lu\n",
		    __wt_page_type_string(dsk->type), (u_long)dsk->u.entries,
		    (u_long)WT_LSN_FILE(dsk->lsn),
		    (u_long)WT_LSN_OFFSET(dsk->lsn));
		break;
	case WT_PAGE_OVFL:
		fprintf(fp, "%s page: data size %lu, lsn %lu/%lu\n",
		    __wt_page_type_string(dsk->type), (u_long)dsk->u.datalen,
		    (u_long)WT_LSN_FILE(dsk->lsn),
		    (u_long)WT_LSN_OFFSET(dsk->lsn));
		break;
	WT_ILLEGAL_FORMAT(session);
	}

	switch (dsk->type) {
	case WT_PAGE_COL_VAR:
	case WT_PAGE_ROW_INT:
	case WT_PAGE_ROW_LEAF:
		ret = __wt_debug_dsk_cell(session, dsk, fp);
		break;
	case WT_PAGE_COL_FIX:
		__wt_debug_dsk_col_fix(btree, dsk, fp);
		break;
	case WT_PAGE_COL_RLE:
		__wt_debug_dsk_col_rle(btree, dsk, fp);
		break;
	case WT_PAGE_COL_INT:
		__wt_debug_dsk_col_int(dsk, fp);
		break;
	default:
		break;
	}

	if (do_close)
		(void)fclose(fp);

	return (ret);
}

#define	WT_DEBUG_TREE_LEAF	0x01			/* Debug leaf pages */
#define	WT_DEBUG_TREE_WALK	0x02			/* Descend the tree */

/*
 * __wt_debug_tree_all --
 *	Dump the in-memory information for a tree, including leaf pages.
 */
int
__wt_debug_tree_all(
    SESSION *session, WT_PAGE *page, const char *ofile, FILE *fp)
{
	int do_close, ret;

	WT_RET(__wt_debug_set_fp(ofile, &fp, &do_close));

	/* A NULL page starts at the top of the tree -- it's a convenience. */
	if (page == NULL)
		page = session->btree->root_page.page;

	ret = __wt_debug_page_work(
	    session, page, fp, WT_DEBUG_TREE_LEAF | WT_DEBUG_TREE_WALK);

	if (do_close)
		(void)fclose(fp);

	return (ret);
}

/*
 * __wt_debug_tree --
 *	Dump the in-memory information for a tree, not including leaf pages.
 */
int
__wt_debug_tree(SESSION *session, WT_PAGE *page, const char *ofile, FILE *fp)
{
	int do_close, ret;

	WT_RET(__wt_debug_set_fp(ofile, &fp, &do_close));

	/* A NULL page starts at the top of the tree -- it's a convenience. */
	if (page == NULL)
		page = session->btree->root_page.page;

	ret = __wt_debug_page_work(session, page, fp, WT_DEBUG_TREE_WALK);

	if (do_close)
		(void)fclose(fp);

	return (ret);
}

/*
 * __wt_debug_page --
 *	Dump the in-memory information for a page.
 */
int
__wt_debug_page(SESSION *session, WT_PAGE *page, const char *ofile, FILE *fp)
{
	int do_close, ret;

	WT_RET(__wt_debug_set_fp(ofile, &fp, &do_close));

	ret = __wt_debug_page_work(session, page, fp, WT_DEBUG_TREE_LEAF);

	if (do_close)
		(void)fclose(fp);

	return (ret);
}

/*
 * __wt_debug_page_work --
 *	Dump the in-memory information for an in-memory page.
 */
static int
__wt_debug_page_work(SESSION *session, WT_PAGE *page, FILE *fp, uint32_t flags)
{
	BTREE *btree;

	btree = session->btree;

	fprintf(fp, "%p: ", page);
	if (WT_PADDR(page) == WT_ADDR_INVALID)
		fprintf(fp, "[not set]");
	else
		fprintf(fp, "[%lu-%lu]",
		    (u_long)WT_PADDR(page),
		    (u_long)WT_PADDR(page) +
		    (WT_OFF_TO_ADDR(btree, WT_PSIZE(page)) - 1));
	fprintf(fp, "/%lu %s",
	    (u_long)WT_PSIZE(page), __wt_page_type_string(page->type));

	switch (page->type) {
	case WT_PAGE_COL_INT:
		fprintf(fp, " recno %llu",
		    (unsigned long long)page->u.col_int.recno);
		break;
	case WT_PAGE_COL_FIX:
	case WT_PAGE_COL_RLE:
	case WT_PAGE_COL_VAR:
		fprintf(fp, " recno %llu",
		    (unsigned long long)page->u.col_leaf.recno);
		break;
	case WT_PAGE_ROW_INT:
	case WT_PAGE_ROW_LEAF:
		break;
	WT_ILLEGAL_FORMAT(session);
	}

	fprintf(fp, " (%s", WT_PAGE_IS_MODIFIED(page) ? "dirty" : "clean");
	if (WT_PAGE_IS_ROOT(page))
		fprintf(fp, ", root");
	if (F_ISSET(page, WT_PAGE_CACHE_COUNTED))
		fprintf(fp, ", cache-counted");
	if (F_ISSET(page, WT_PAGE_DELETED))
		fprintf(fp, ", deleted");
	if (F_ISSET(page, WT_PAGE_PINNED))
		fprintf(fp, ", pinned");
	if (F_ISSET(page, WT_PAGE_SPLIT))
		fprintf(fp, ", split");
	fprintf(fp, ")\n");

	/* Dump the page. */
	switch (page->type) {
	case WT_PAGE_COL_FIX:
		if (LF_ISSET(WT_DEBUG_TREE_LEAF))
			__wt_debug_page_col_fix(session, page, fp);
		break;
	case WT_PAGE_COL_INT:
		WT_RET(__wt_debug_page_col_int(session, page, fp, flags));
		break;
	case WT_PAGE_COL_RLE:
		if (LF_ISSET(WT_DEBUG_TREE_LEAF))
			__wt_debug_page_col_rle(session, page, fp);
		break;
	case WT_PAGE_COL_VAR:
		if (LF_ISSET(WT_DEBUG_TREE_LEAF))
			WT_RET(__wt_debug_page_col_var(session, page, fp));
		break;
	case WT_PAGE_ROW_LEAF:
		if (LF_ISSET(WT_DEBUG_TREE_LEAF))
			WT_RET(__wt_debug_page_row_leaf(session, page, fp));
		break;
	case WT_PAGE_ROW_INT:
		WT_RET(__wt_debug_page_row_int(session, page, fp, flags));
		break;
	WT_ILLEGAL_FORMAT(session);
	}

	return (0);
}

/*
 * __wt_debug_page_col_fix --
 *	Dump an in-memory WT_PAGE_COL_FIX page.
 */
static void
__wt_debug_page_col_fix(SESSION *session, WT_PAGE *page, FILE *fp)
{
	WT_COL *cip;
	WT_UPDATE *upd;
	uint32_t fixed_len, i;
	void *cipvalue;

	fixed_len = session->btree->fixed_len;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_COL_FOREACH(page, cip, i) {
		cipvalue = WT_COL_PTR(page, cip);
		fprintf(fp, "\tV {");
		if (WT_FIX_DELETE_ISSET(cipvalue))
			fprintf(fp, "deleted");
		else
			__wt_print_byte_string(cipvalue, fixed_len, fp);
		fprintf(fp, "}\n");

		if ((upd = WT_COL_UPDATE(page, cip)) != NULL)
			__wt_debug_update(upd, fp);
	}
}

/*
 * __wt_debug_page_col_int --
 *	Dump an in-memory WT_PAGE_COL_INT page.
 */
static int
__wt_debug_page_col_int(
    SESSION *session, WT_PAGE *page, FILE *fp, uint32_t flags)
{
	WT_COL_REF *cref;
	uint32_t i;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_COL_REF_FOREACH(page, cref, i) {
		fprintf(fp,
		    "\trecno %llu, ", (unsigned long long)cref->recno);
		__wt_debug_ref(&cref->ref, fp);
	}

	if (!LF_ISSET(WT_DEBUG_TREE_WALK))
		return (0);

	WT_COL_REF_FOREACH(page, cref, i)
		if (WT_COL_REF_STATE(cref) == WT_REF_MEM)
			WT_RET(__wt_debug_page_work(
			    session, WT_COL_REF_PAGE(cref), fp, flags));
	return (0);
}

/*
 * __wt_debug_page_col_rle --
 *	Dump an in-memory WT_PAGE_COL_RLE page.
 */
static void
__wt_debug_page_col_rle(SESSION *session, WT_PAGE *page, FILE *fp)
{
	WT_COL *cip;
	WT_INSERT *ins;
	uint32_t fixed_len, i;
	void *cipvalue;

	fixed_len = session->btree->fixed_len;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_COL_FOREACH(page, cip, i) {
		cipvalue = WT_COL_PTR(page, cip);
		fprintf(fp,
		    "\trepeat %lu {", (u_long)WT_RLE_REPEAT_COUNT(cipvalue));
		if (WT_FIX_DELETE_ISSET(WT_RLE_REPEAT_DATA(cipvalue)))
			fprintf(fp, "deleted");
		else
			__wt_print_byte_string(
			    WT_RLE_REPEAT_DATA(cipvalue), fixed_len, fp);
		fprintf(fp, "}\n");

		if ((ins = WT_COL_INSERT(page, cip)) != NULL)
			__wt_debug_col_insert(ins, fp);
	}
}

/*
 * __wt_debug_page_col_var --
 *	Dump an in-memory WT_PAGE_COL_VAR page.
 */
static int
__wt_debug_page_col_var(SESSION *session, WT_PAGE *page, FILE *fp)
{
	WT_COL *cip;
	WT_UPDATE *upd;
	uint32_t i;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_COL_FOREACH(page, cip, i) {
		fprintf(fp, "\tV {");
		WT_RET(
		    __wt_debug_cell_data(session, WT_COL_PTR(page, cip), fp));
		fprintf(fp, "}\n");

		if ((upd = WT_COL_UPDATE(page, cip)) != NULL)
			__wt_debug_update(upd, fp);
	}
	return (0);
}

/*
 * __wt_debug_page_row_int --
 *	Dump an in-memory WT_PAGE_ROW_INT page.
 */
static int
__wt_debug_page_row_int(
    SESSION *session, WT_PAGE *page, FILE *fp, uint32_t flags)
{
	WT_ROW_REF *rref;
	uint32_t i;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_ROW_REF_FOREACH(page, rref, i) {
		if (__wt_key_process(rref))
			fprintf(fp, "\tK: {requires processing}\n");
		else
			__wt_debug_item("\tK", rref, fp);
		fprintf(fp, "\t");
		__wt_debug_ref(&rref->ref, fp);
	}

	if (!LF_ISSET(WT_DEBUG_TREE_WALK))
		return (0);

	WT_ROW_REF_FOREACH(page, rref, i)
		if (WT_ROW_REF_STATE(rref) == WT_REF_MEM)
			WT_RET(__wt_debug_page_work(
			    session, WT_ROW_REF_PAGE(rref), fp, flags));
	return (0);
}

/*
 * __wt_debug_page_row_leaf --
 *	Dump an in-memory WT_PAGE_ROW_LEAF page.
 */
static int
__wt_debug_page_row_leaf(SESSION *session, WT_PAGE *page, FILE *fp)
{
	WT_INSERT *ins;
	WT_ROW *rip;
	WT_UPDATE *upd;
	uint32_t i;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	/*
	 * Dump any K/V pairs inserted into the page before the first from-disk
	 * key on the page.
	 */
	if ((ins = WT_ROW_INSERT_SMALLEST(page)) != NULL)
		__wt_debug_row_insert(ins, fp);

	/* Dump the page's K/V pairs. */
	WT_ROW_FOREACH(page, rip, i) {
		if (__wt_key_process(rip))
			fprintf(fp, "\tK: {requires processing}\n");
		else
			__wt_debug_item("\tK", rip, fp);

		fprintf(fp, "\tV: {");
		if (!WT_ROW_EMPTY_ISSET(rip))
			WT_RET(__wt_debug_cell_data(
			    session, WT_ROW_PTR(page, rip), fp));
		fprintf(fp, "}\n");

		if ((upd = WT_ROW_UPDATE(page, rip)) != NULL)
			__wt_debug_update(upd, fp);

		if ((ins = WT_ROW_INSERT(page, rip)) != NULL)
			__wt_debug_row_insert(ins, fp);
	}

	return (0);
}

/*
 * __wt_debug_col_insert --
 *	Dump an RLE column-store insert array.
 */
static void
__wt_debug_col_insert(WT_INSERT *ins, FILE *fp)
{
	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	for (; ins != NULL; ins = ins->next) {
		fprintf(fp, "\tinsert %llu\n",
		    (unsigned long long)WT_INSERT_RECNO(ins));
		__wt_debug_update(ins->upd, fp);
	}
}

/*
 * __wt_debug_row_insert --
 *	Dump an insert array.
 */
static void
__wt_debug_row_insert(WT_INSERT *ins, FILE *fp)
{
	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	for (; ins != NULL; ins = ins->next) {
		__wt_debug_pair("\tinsert",
		    WT_INSERT_KEY(ins), WT_INSERT_KEY_SIZE(ins), fp);
		__wt_debug_update(ins->upd, fp);
	}
}

/*
 * __wt_debug_update --
 *	Dump an update array.
 */
static void
__wt_debug_update(WT_UPDATE *upd, FILE *fp)
{
	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	for (; upd != NULL; upd = upd->next)
		if (WT_UPDATE_DELETED_ISSET(upd))
			fprintf(fp, "\tupdate: {deleted}\n");
		else
			__wt_debug_pair(
			    "\tupdate", WT_UPDATE_DATA(upd), upd->size, fp);
}

/*
 * __wt_debug_dsk_cell --
 *	Dump a page of WT_CELL's.
 */
static int
__wt_debug_dsk_cell(SESSION *session, WT_PAGE_DISK *dsk, FILE *fp)
{
	WT_CELL *cell;
	uint32_t i;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_CELL_FOREACH(dsk, cell, i)
		WT_RET(__wt_debug_cell(session, cell, fp));
	return (0);
}

/*
 * __wt_debug_cell --
 *	Dump a single WT_CELL.
 */
static int
__wt_debug_cell(SESSION *session, WT_CELL *cell, FILE *fp)
{
	WT_OFF off;
	WT_OFF_RECORD off_record;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	fprintf(fp, "\t%s: len %lu",
	    __wt_cell_type_string(cell), (u_long)__wt_cell_datalen(cell));

	switch (__wt_cell_type(cell)) {
	case WT_CELL_DATA:
	case WT_CELL_DEL:
	case WT_CELL_KEY:
		break;
	case WT_CELL_DATA_OVFL:
	case WT_CELL_KEY_OVFL:
	case WT_CELL_OFF:
		__wt_cell_off(cell, &off);
		fprintf(fp, ", offpage: addr %lu, size %lu",
		    (u_long)off.addr, (u_long)off.size);
		break;
	case WT_CELL_OFF_RECORD:
		__wt_cell_off_record(cell, &off_record);
		fprintf(fp,
		    ", offpage: addr %lu, size %lu, starting recno %llu",
		    (u_long)off_record.addr, (u_long)off_record.size,
		    (unsigned long long)WT_RECNO(&off_record));
		break;
	WT_ILLEGAL_FORMAT(session);
	}

	fprintf(fp, "\n\t{");
	WT_RET(__wt_debug_cell_data(session, cell, fp));
	fprintf(fp, "}\n");
	return (0);
}

/*
 * __wt_debug_dsk_col_int --
 *	Dump a WT_PAGE_COL_INT page.
 */
static void
__wt_debug_dsk_col_int(WT_PAGE_DISK *dsk, FILE *fp)
{
	WT_OFF_RECORD *off_record;
	uint32_t i;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_OFF_FOREACH(dsk, off_record, i)
		fprintf(fp,
		    "\toffpage: addr %lu, size %lu, starting recno %llu\n",
		    (u_long)off_record->addr, (u_long)off_record->size,
		    (unsigned long long)WT_RECNO(off_record));
}

/*
 * __wt_debug_dsk_col_fix --
 *	Dump a WT_PAGE_COL_FIX page.
 */
static void
__wt_debug_dsk_col_fix(BTREE *btree, WT_PAGE_DISK *dsk, FILE *fp)
{
	uint32_t i;
	uint8_t *p;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_FIX_FOREACH(btree, dsk, p, i) {
		fprintf(fp, "\t{");
		if (WT_FIX_DELETE_ISSET(p))
			fprintf(fp, "deleted");
		else
			__wt_print_byte_string(p, btree->fixed_len, fp);
		fprintf(fp, "}\n");
	}
}

/*
 * __wt_debug_dsk_col_rle --
 *	Dump a WT_PAGE_COL_RLE page.
 */
static void
__wt_debug_dsk_col_rle(BTREE *btree, WT_PAGE_DISK *dsk, FILE *fp)
{
	uint32_t i;
	uint8_t *p;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	WT_RLE_REPEAT_FOREACH(btree, dsk, p, i) {
		fprintf(fp, "\trepeat %lu {",
		    (u_long)WT_RLE_REPEAT_COUNT(p));
		if (WT_FIX_DELETE_ISSET(WT_RLE_REPEAT_DATA(p)))
			fprintf(fp, "deleted");
		else
			__wt_print_byte_string(
			    WT_RLE_REPEAT_DATA(p), btree->fixed_len, fp);
		fprintf(fp, "}\n");
	}
}

/*
 * __wt_debug_cell_data --
 *	Dump a single cell's data in debugging mode.
 */
static int
__wt_debug_cell_data(SESSION *session, const WT_CELL *cell, FILE *fp)
{
	BTREE *btree;
	WT_BUF *tmp;
	uint32_t size;
	const uint8_t *p;
	int ret;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	btree = session->btree;
	tmp = NULL;
	ret = 0;

	switch (__wt_cell_type(cell)) {
	case WT_CELL_KEY:
		if (btree->huffman_key != NULL)
			goto process;
		goto onpage;
	case WT_CELL_DATA:
		if (btree->huffman_value != NULL)
			goto process;
onpage:		__wt_cell_data_and_len(cell, &p, &size);
		break;
	case WT_CELL_KEY_OVFL:
	case WT_CELL_DATA_OVFL:
process:	WT_ERR(__wt_scr_alloc(session, 0, &tmp));
		WT_ERR(__wt_cell_process(session, cell, tmp));
		p = tmp->data;
		size = tmp->size;
		break;
	case WT_CELL_DEL:
		p = (uint8_t *)"deleted";
		size = 7;
		break;
	case WT_CELL_OFF:
		p = (uint8_t *)"offpage";
		size = sizeof("offpage") - 1;
		break;
	case WT_CELL_OFF_RECORD:
		p = (uint8_t *)"offpage_record";
		size = sizeof("offpage_record") - 1;
		break;
	WT_ILLEGAL_FORMAT_ERR(session, ret);
	}

	__wt_print_byte_string(p, size, fp);

err:	if (tmp != NULL)
		__wt_scr_release(&tmp);
	return (ret);
}

/*
 * __wt_debug_item --
 *	Dump a single WT_ITEM in debugging mode, with an optional tag.
 */
void
__wt_debug_item(const char *tag, void *arg_item, FILE *fp)
{
	WT_ITEM *item;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	/*
	 * The argument isn't necessarily a WT_ITEM structure, but the first two
	 * fields of the argument are always a void *data/uint32_t size pair.
	 */
	item = arg_item;
	__wt_debug_pair(tag, item->data, item->size, fp);
}

/*
 * __wt_debug_pair --
 *	Dump a single data/size pair, with an optional tag.
 */
static void
__wt_debug_pair(const char *tag, const void *data, uint32_t size, FILE *fp)
{
	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	if (tag != NULL)
		fprintf(fp, "%s: ", tag);
	fprintf(fp, "%lu {",  (u_long)size);
	__wt_print_byte_string(data, size, fp);
	fprintf(fp, "}\n");
}

/*
 * __wt_debug_ref --
 *	Print out a page's in-memory WT_REF state.
 */
static void
__wt_debug_ref(WT_REF *ref, FILE *fp)
{
	const char *s;

	if (fp == NULL)				/* Default to stderr */
		fp = stderr;

	switch (ref->state) {
	case WT_REF_DISK:
		s = "disk";
		break;
	case WT_REF_LOCKED:
		s = "locked";
		break;
	case WT_REF_MEM:
		s = "memory";
		break;
	default:
		s = "UNKNOWN";
		break;
	}

	if (ref->addr == WT_ADDR_INVALID)
		fprintf(fp, "not-set");
	else
		fprintf(fp, "%lu", (u_long)ref->addr);

	fprintf(fp, "/%lu: %s", (u_long)ref->size, s);
	if (ref->state == WT_REF_MEM)
		fprintf(fp, "(%p)", ref->page);
	fprintf(fp, "\n");
}
#endif
