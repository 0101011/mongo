/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008 WiredTiger Software.
 *	All rights reserved.
 *
 * $Id$
 */

#include "wt_internal.h"

#ifdef HAVE_DIAGNOSTIC
static int  __wt_bt_dump_addr(DB *, u_int32_t, char *, FILE *);
static void __wt_bt_dump_dbt(DBT *, FILE *);
static void __wt_bt_dump_item(WT_TOC *, WT_ITEM *, FILE *);
static void __wt_bt_dump_item_data(WT_TOC *, WT_ITEM *, FILE *);

int
__wt_diag_set_fp(const char *ofile, FILE **fpp, int *close_varp)
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
 * __wt_bt_dump_debug --
 *	Dump a database in debugging mode.
 */
int
__wt_bt_dump_debug(DB *db, char *ofile, FILE *fp)
{
	int do_close, ret;

	WT_RET(__wt_diag_set_fp(ofile, &fp, &do_close));

	/*
	 * We use the verification code to do debugging dumps because if we're
	 * dumping in debugging mode, we want to confirm the page is OK before
	 * walking it.
	 */
	ret = __wt_db_verify_int(db, NULL, fp, 0);

	if (do_close)
		(void)fclose(fp);

	return (ret);
}

/*
 * __wt_bt_dump_addr --
 *	Dump a single page in debugging mode.
 */
static int
__wt_bt_dump_addr(DB *db, u_int32_t addr, char *ofile, FILE *fp)
{
	ENV *env;
	WT_PAGE *page;
	WT_TOC *toc;
	u_int32_t bytes;
	int ret;

	env = db->env;

	WT_RET(env->toc(env, 0, &toc));
	WT_TOC_DB_INIT(toc, db, "dump addr");

	/*
	 * Read in a single fragment.   If we get the page from the cache,
	 * it will be correct, and we can use it without further concern.
	 * If we don't get the page from the cache, figure out the type of
	 * the page and get it for real.
	 *
	 * We don't have any way to test if a page was found in the cache,
	 * so we check the in-memory page information -- pages in the cache
	 * should have in-memory page information.
	 */
	WT_RET(__wt_cache_in(toc,
	    addr, (u_int32_t)WT_FRAGMENT, WT_UNFORMATTED, &page));
	if (page->indx_count == 0) {
		switch (page->hdr->type) {
		case WT_PAGE_OVFL:
			bytes =
			    WT_OVFL_BYTES(db, page->hdr->u.datalen);
			break;
		case WT_PAGE_INT:
		case WT_PAGE_DUP_INT:
			bytes = db->intlsize;
			break;
		case WT_PAGE_LEAF:
		case WT_PAGE_DUP_LEAF:
			bytes = db->leafsize;
			break;
		WT_DEFAULT_FORMAT(db);
		}
		WT_RET(__wt_cache_out(toc, page, 0));
		WT_RET(__wt_cache_in(toc, addr, bytes, 0, &page));
	}

	ret = __wt_bt_dump_page(db, page, ofile, fp, 0);

	WT_TRET(__wt_cache_out(toc, page, 0));
	WT_TRET(toc->close(toc, 0));

	return (ret);
}

/*
 * __wt_bt_dump_page --
 *	Dump a single in-memory page in debugging mode.
 */
int
__wt_bt_dump_page(DB *db, WT_PAGE *page, char *ofile, FILE *fp, int inmemory)
{
	ENV *env;
	WT_INDX *ip;
	WT_ITEM *item;
	WT_PAGE_HDR *hdr;
	WT_TOC *toc;
	u_long icnt;
	u_int32_t i;
	u_int8_t *p;
	int do_close, ret;

	env = db->env;
	ret = 0;

	WT_RET(__wt_diag_set_fp(ofile, &fp, &do_close));

	WT_ERR(env->toc(env, 0, &toc));
	WT_TOC_DB_INIT(toc, db, "dump page");

	fprintf(fp, "addr: %lu-%lu {\n", (u_long)page->addr,
	    (u_long)page->addr + (WT_OFF_TO_ADDR(db, page->bytes) - 1));

	if (page->addr == 0)
		__wt_bt_desc_dump(page, fp);

	fprintf(fp,
	    "addr %lu, bytes %lu\n", (u_long)page->addr, (u_long)page->bytes);
	fprintf(fp, "first-free %#lx, space avail: %lu, records: %llu\n",
	    (u_long)page->first_free, (u_long)page->space_avail, page->records);

	hdr = page->hdr;
	fprintf(fp, "%s: ", __wt_bt_hdr_type(hdr));
	if (hdr->type == WT_PAGE_OVFL)
		fprintf(fp, "%lu bytes", (u_long)hdr->u.datalen);
	else
		fprintf(fp, "%lu entries", (u_long)hdr->u.entries);
	fprintf(fp,
	    ", lsn %lu/%lu\n", (u_long)hdr->lsn[0], (u_long)hdr->lsn[1]);
	if (hdr->prntaddr == WT_ADDR_INVALID)
		fprintf(fp, "prntaddr (none), ");
	else
		fprintf(fp, "prntaddr %lu, ", (u_long)hdr->prntaddr);
	if (hdr->prevaddr == WT_ADDR_INVALID)
		fprintf(fp, "prevaddr (none), ");
	else
		fprintf(fp, "prevaddr %lu, ", (u_long)hdr->prevaddr);
	if (hdr->nextaddr == WT_ADDR_INVALID)
		fprintf(fp, "nextaddr (none)");
	else
		fprintf(fp, "nextaddr %lu", (u_long)hdr->nextaddr);
	fprintf(fp, "\n}\n");

	if (hdr->type == WT_PAGE_OVFL)
		return (0);

	/* Optionally dump the in-memory page, vs. the on-disk page. */
	if (inmemory) {
		icnt = 0;
		WT_INDX_FOREACH(page, ip, i) {
			fprintf(fp, "%6lu: {flags %#lx}\n",
			    ++icnt, (u_long)ip->flags);
			if (ip->data != NULL) {
				fprintf(fp, "\tdbt: ");
				__wt_bt_dump_dbt((DBT *)ip, fp);
			}
			if (ip->ditem != NULL) {
				fprintf(fp, "\tditem: ");
				__wt_bt_dump_item(toc, ip->ditem, fp);
			}
		}
	} else
		for (p = WT_PAGE_BYTE(page), i = 1; i <= hdr->u.entries; ++i) {
			item = (WT_ITEM *)p;
			fprintf(fp, "%6lu: {type %s; len %lu; off %lu}\n\t{",
			    (u_long)i, __wt_bt_item_type(item),
			    (u_long)WT_ITEM_LEN(item),
			    (u_long)(p - (u_int8_t *)hdr));
			__wt_bt_dump_item_data(toc, item, fp);
			fprintf(fp, "}\n");
			p += WT_ITEM_SPACE_REQ(WT_ITEM_LEN(item));
		}
	fprintf(fp, "\n");

	WT_TRET(toc->close(toc, 0));

err:	if (do_close)
		(void)fclose(fp);

	return (ret);
}

/*
 * __wt_bt_dump_item --
 *	Dump a single item in debugging mode.
 */
static void
__wt_bt_dump_item(WT_TOC *toc, WT_ITEM *item, FILE *fp)
{
	if (fp == NULL)
		fp = stdout;

	fprintf(fp, "%s {", __wt_bt_item_type(item));

	__wt_bt_dump_item_data(toc, item, fp);

	fprintf(fp, "}\n");
}

/*
 * __wt_bt_dump_item_data --
 *	Dump a single item's data in debugging mode.
 */
static void
__wt_bt_dump_item_data(WT_TOC *toc, WT_ITEM *item, FILE *fp)
{
	WT_ITEM_OVFL *ovfl;
	WT_ITEM_OFFP *offp;
	WT_PAGE *page;

	switch (WT_ITEM_TYPE(item)) {
	case WT_ITEM_KEY:
	case WT_ITEM_DATA:
	case WT_ITEM_DUP:
		__wt_bt_print(WT_ITEM_BYTE(item), WT_ITEM_LEN(item), fp);
		break;
	case WT_ITEM_KEY_OVFL:
	case WT_ITEM_DATA_OVFL:
	case WT_ITEM_DUP_OVFL:
		ovfl = (WT_ITEM_OVFL *)WT_ITEM_BYTE(item);
		fprintf(fp, "addr %lu; len %lu; ",
		    (u_long)ovfl->addr, (u_long)ovfl->len);

		if (__wt_bt_ovfl_in(toc,
		    ovfl->addr, (u_int32_t)ovfl->len, &page) == 0) {
			__wt_bt_print(WT_PAGE_BYTE(page), ovfl->len, fp);
			(void)__wt_bt_page_out(toc, page, 0);
		}
		break;
	case WT_ITEM_OFFP_INTL:
	case WT_ITEM_OFFP_LEAF:
		offp = (WT_ITEM_OFFP *)WT_ITEM_BYTE(item);
		fprintf(fp, "addr: %lu, records %llu",
		    (u_long)offp->addr, WT_64_CAST(offp->records));
		break;
	default:
		fprintf(fp, "unsupported type");
		break;
	}
}

/*
 * __wt_bt_dump_dbt --
 *	Dump a single DBT in debugging mode.
 */
static void
__wt_bt_dump_dbt(DBT *dbt, FILE *fp)
{
	if (fp == NULL)
		fp = stdout;
	fprintf(fp, "{");
	__wt_bt_print(dbt->data, dbt->size, fp);
	fprintf(fp, "}\n");
}
#endif
