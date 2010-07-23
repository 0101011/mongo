/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008 WiredTiger Software.
 *	All rights reserved.
 *
 * $Id$
 */

#include "wt_internal.h"

static void __wt_bt_page_discard_expcol(ENV *, WT_PAGE *);
static void __wt_bt_page_discard_repl(ENV *, WT_PAGE *);
static void __wt_bt_page_discard_repl_list(ENV *, WT_REPL *);

/*
 * __wt_bt_page_discard --
 *	Free all memory associated with a page.
 */
void
__wt_bt_page_discard(ENV *env, WT_PAGE *page)
{
	WT_ROW *rip;
	u_int32_t i;
	void *last_key;

	WT_ASSERT(env, page->modified == 0);
	WT_ENV_FCHK_ASSERT(
	    env, "__wt_bt_page_discard", page->flags, WT_APIMASK_WT_PAGE);

	/* Free the on-disk index array. */
	switch (page->hdr->type) {
	case WT_PAGE_DUP_INT:
	case WT_PAGE_DUP_LEAF:
	case WT_PAGE_ROW_INT:
	case WT_PAGE_ROW_LEAF:
		/*
		 * For each entry, see if the key was an allocation, that is,
		 * if it points somewhere other than the original page.  If it
		 * is an allocation, free it.
		 *
		 * Only handle the first key entry for duplicate key/data pairs,
		 * the others reference the same memory.
		 */
		last_key = NULL;
		WT_INDX_FOREACH(page, rip, i) {
			if (rip->key == last_key)
				continue;
			last_key = rip->key;
			if (!WT_ROW_KEY_ON_PAGE(page, rip))
				__wt_free(env, rip->key, rip->size);
		}
		break;
	case WT_PAGE_COL_FIX:
	case WT_PAGE_COL_INT:
	case WT_PAGE_COL_VAR:
	case WT_PAGE_DESCRIPT:
	case WT_PAGE_OVFL:
	default:
		break;
	}
	if (page->u.indx != NULL)
		__wt_free(env, page->u.indx, 0);

	/* Free the modified/deletion replacements array. */
	if (page->repl != NULL)
		__wt_bt_page_discard_repl(env, page);

	/* Free the repeat-count compressed column store expansion array. */
	if (page->expcol != NULL)
		__wt_bt_page_discard_expcol(env, page);

	__wt_free(env, page->hdr, page->size);
	__wt_free(env, page, sizeof(WT_PAGE));
}

/*
 * __wt_bt_page_discard_repl --
 *	Discard the replacement array.
 */
static void
__wt_bt_page_discard_repl(ENV *env, WT_PAGE *page)
{
	WT_REPL **replp;
	u_int i;

	/*
	 * For each non-NULL slot in the page's array of replacements, free the
	 * linked list anchored in that slot.
	 */
	WT_REPL_FOREACH(page, replp, i)
		if (*replp != NULL)
			__wt_bt_page_discard_repl_list(env, *replp);

	/* Free the page's array of replacements. */
	__wt_free(env, page->repl, page->indx_count * sizeof(WT_REPL *));
}

/*
 * __wt_bt_page_discard_expcol --
 *	Discard the repeat-count compressed column store expansion array.
 */
static void
__wt_bt_page_discard_expcol(ENV *env, WT_PAGE *page)
{
	WT_COL_EXPAND **expp, *exp, *a;
	u_int i;

	/*
	 * For each non-NULL slot in the page's repeat-count compressed column
	 * store expansion array, free the linked list of WT_COL_EXPAND
	 * structures anchored in that slot.
	 */
	WT_EXPCOL_FOREACH(page, expp, i) {
		if ((exp = *expp) == NULL)
			continue;
		/*
		 * Free the linked list of WT_REPL structures anchored in the
		 * WT_COL_EXPAND entry.
		 */
		__wt_bt_page_discard_repl_list(env, exp->repl);
		do {
			a = exp->next;
			__wt_free(env, exp, sizeof(WT_COL_EXPAND));
		} while ((exp = a) != NULL);
	}

	/* Free the page's expansion array. */
	__wt_free(
	    env, page->expcol, page->indx_count * sizeof(WT_COL_EXPAND *));
}

/*
 * __wt_bt_page_discard_repl_list --
 *	Walk a WT_REPL forward-linked list and free the per-thread combination
 *	of a WT_REPL structure and its associated data.
 */
static void
__wt_bt_page_discard_repl_list(ENV *env, WT_REPL *repl)
{
	WT_DATA_UPDATE *upd;
	WT_REPL *a;

	do {
		a = repl->next;

		/*
		 * The bytes immediately before the WT_REPL structure are a
		 * pointer to the per-thread WT_DATA_UPDATE structure in
		 * which this WT_REPL and its associated data are stored.
		 */
		upd = *(WT_DATA_UPDATE **)
		    ((u_int8_t *)repl - sizeof(WT_DATA_UPDATE *));
		WT_ASSERT(env, upd->out < upd->in);
		if (++upd->out == upd->in)
			__wt_free(env, upd, upd->len);
	} while ((repl = a) != NULL);
}
