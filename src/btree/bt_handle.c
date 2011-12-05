/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

static int __btree_alloc(WT_SESSION_IMPL *, const char *, const char *);
static int __btree_conf(WT_SESSION_IMPL *, const char *);
static int __btree_read_meta(WT_SESSION_IMPL *, const char *[], uint32_t);
static int __btree_last(WT_SESSION_IMPL *);
static int __btree_page_sizes(WT_SESSION_IMPL *, const char *);

static int pse1(WT_SESSION_IMPL *, const char *, uint32_t, uint32_t);
static int pse2(WT_SESSION_IMPL *, const char *, uint32_t, uint32_t, uint32_t);

/*
 * __wt_btree_create --
 *	Create a Btree.
 */
int
__wt_btree_create(WT_SESSION_IMPL *session, const char *filename)
{
	WT_FH *fh;
	int exist, ret;

	/* Check to see if the file exists -- we don't want to overwrite it. */
	WT_RET(__wt_exist(session, filename, &exist));
	if (exist) {
		__wt_errx(session,
		    "the file %s already exists; to re-create it, remove it "
		    "first, then create it",
		    filename);
		return (WT_ERROR);
	}

	/* Open the underlying file handle. */
	WT_RET(__wt_open(session, filename, 0666, 1, &fh));

	/* Write out the file's meta-data. */
	ret = __wt_desc_write(session, fh);

	/* Close the file handle. */
	WT_TRET(__wt_close(session, fh));

	return (ret);
}

/*
 * __wt_btree_open --
 *	Open a Btree.
 */
int
__wt_btree_open(WT_SESSION_IMPL *session,
    const char *name, const char *filename,
    const char *config, const char *cfg[], uint32_t flags)
{
	WT_BTREE *btree;
	WT_CONNECTION_IMPL *conn;
	int matched, ret;

	conn = S2C(session);

	/*
	 * The file configuration string must point to allocated memory: it
	 * is stored in the returned btree handle and freed when the handle
	 * is closed.
	 */

	/* Increment the reference count if we already have the btree open. */
	matched = 0;
	__wt_spin_lock(session, &conn->spinlock);
	TAILQ_FOREACH(btree, &conn->btqh, q) {
		if (strcmp(filename, btree->filename) == 0) {
			++btree->refcnt;
			session->btree = btree;
			matched = 1;
			break;
		}
	}
	if (matched) {
		__wt_spin_unlock(session, &conn->spinlock);

		/* Check that the handle is open. */
		__wt_readlock(session, btree->rwlock);
		matched = F_ISSET(btree, WT_BTREE_OPEN);
		__wt_rwunlock(session, btree->rwlock);

		if (!matched) {
			__wt_writelock(session, btree->rwlock);
			if (!F_ISSET(btree, WT_BTREE_OPEN)) {
				/* We're going to overwrite the old config. */
				__wt_free(session, btree->config);
				goto conf;
			}

			/* It was opened while we waited. */
			__wt_rwunlock(session, btree->rwlock);
		}

		/* The config string will not be needed: free it now. */
		__wt_free(session, config);
		return (0);
	}

	/* Allocate the WT_BTREE structure. */
	if ((ret = __btree_alloc(session, name, filename)) == 0) {
		btree = session->btree;

		/* Lock the handle before it is inserted in the list. */
		__wt_writelock(session, btree->rwlock);

		/* Add to the connection list. */
		btree->refcnt = 1;
		TAILQ_INSERT_TAIL(&conn->btqh, btree, q);
		++conn->btqcnt;
	}
	__wt_spin_unlock(session, &conn->spinlock);

	if (ret != 0)
		return (ret);

	/* Initialize and configure the WT_BTREE structure. */
conf:	WT_ERR(__btree_conf(session, config));

	/* Open the underlying file handle. */
	WT_ERR(__wt_open(session, filename, 0666, 1, &btree->fh));

	/* Read in the root page. */
	WT_ERR(__btree_read_meta(session, cfg, flags));

	WT_STAT_INCR(conn->stats, file_open);

	if (0) {
err:		if (btree->fh != NULL) {
			(void)__wt_close(session, btree->fh);
			btree->fh = NULL;
		}
		F_CLR(btree, WT_BTREE_OPEN);
	}

	__wt_rwunlock(session, btree->rwlock);
	return (ret);
}

/*
 * __btree_alloc --
 *	Allocate a WT_BTREE structure.
 */
static int
__btree_alloc(WT_SESSION_IMPL *session, const char *name, const char *filename)
{
	WT_BTREE *btree;

	WT_RET(__wt_calloc_def(session, 1, &btree));
	session->btree = btree;

	WT_RET(__wt_rwlock_alloc(session, "btree handle", &btree->rwlock));
        __wt_spin_init(session, &btree->freelist_lock);

	/* Take copies of names for the new handle. */
	WT_RET(__wt_strdup(session, name, &btree->name));
	WT_RET(__wt_strdup(session, filename, &btree->filename));

	return (0);
}

/*
 * __btree_conf --
 *	Configure a WT_BTREE structure.
 */
static int
__btree_conf(WT_SESSION_IMPL *session, const char *config)
{
	WT_BTREE *btree;
	WT_CONFIG_ITEM cval;
	WT_CONNECTION_IMPL *conn;
	WT_NAMED_COLLATOR *ncoll;
	WT_NAMED_COMPRESSOR *ncomp;
	uint32_t bitcnt;
	int fixed;

	btree = session->btree;
	conn = S2C(session);

	/* Validate file types and check the data format plan. */
	WT_RET(__wt_config_getones(session, config, "key_format", &cval));
	WT_RET(__wt_struct_check(session, cval.str, cval.len, NULL, NULL));
	if (cval.len > 0 && strncmp(cval.str, "r", cval.len) == 0)
		btree->type = BTREE_COL_VAR;
	else
		btree->type = BTREE_ROW;
	WT_RET(__wt_strndup(session, cval.str, cval.len, &btree->key_format));

	WT_RET(__wt_config_getones(session, config, "value_format", &cval));
	WT_RET(__wt_strndup(session, cval.str, cval.len, &btree->value_format));

	/* Row-store key comparison and key gap for prefix compression. */
	if (btree->type == BTREE_ROW) {
		WT_RET(__wt_config_getones(
		    session, config, "collator", &cval));
		if (cval.len > 0) {
			TAILQ_FOREACH(ncoll, &conn->collqh, q) {
				if (strncmp(
				    ncoll->name, cval.str, cval.len) == 0) {
					btree->collator = ncoll->collator;
					break;
				}
			}
			if (btree->collator == NULL) {
				__wt_errx(session, "unknown collator '%.*s'",
				    (int)cval.len, cval.str);
				return (EINVAL);
			}
		}
		WT_RET(__wt_config_getones(
		    session, config, "key_gap", &cval));
		btree->key_gap = (uint32_t)cval.val;
	}
	/* Check for fixed-size data. */
	if (btree->type == BTREE_COL_VAR) {
		WT_RET(__wt_struct_check(
		    session, cval.str, cval.len, &fixed, &bitcnt));
		if (fixed) {
			if (bitcnt == 0 || bitcnt > 8) {
				__wt_errx(session,
				    "the fixed-width field size must be "
				    "greater than 0 and less than or equal "
				    "to 8");
				return (WT_ERROR);
			}
			btree->bitcnt = (uint8_t)bitcnt;
			btree->type = BTREE_COL_FIX;
		}
	}

	/* Page sizes */
	WT_RET(__btree_page_sizes(session, config));

	/* Huffman encoding */
	WT_RET(__wt_btree_huffman_open(session, config));

	/* Page compressor */
	WT_RET(__wt_config_getones(
	    session, config, "block_compressor", &cval));
	if (cval.len > 0) {
		TAILQ_FOREACH(ncomp, &conn->compqh, q) {
			if (strncmp(ncomp->name, cval.str, cval.len) == 0) {
				btree->compressor = ncomp->compressor;
				break;
			}
		}
		if (btree->compressor == NULL) {
			__wt_errx(session, "unknown block_compressor '%.*s'",
			    (int)cval.len, cval.str);
			return (EINVAL);
		}
	}

	btree->root_page.addr = WT_ADDR_INVALID;

	TAILQ_INIT(&btree->freeqa);
	TAILQ_INIT(&btree->freeqs);
	btree->free_addr = WT_ADDR_INVALID;

	WT_RET(__wt_stat_alloc_btree_stats(session, &btree->stats));

	/* Take the config string: it will be freed with the btree handle. */
	btree->config = config;

	return (0);
}

/*
 * __wt_btree_root_empty_init --
 *      Create an empty in-memory tree.
 */
int
__wt_btree_root_empty_init(WT_SESSION_IMPL *session)
{
	WT_BTREE *btree;
	WT_PAGE *page;
	WT_REF *ref;

	btree = session->btree;

	/*
	 * A note about empty trees: the initial tree is a root page and a leaf
	 * page, neither of which are marked dirty.   If evicted without being
	 * modified, that's OK, nothing will ever be written.
	 *
	 * Create the empty root page.
	 */
	WT_RET(__wt_calloc_def(session, 1, &page));
	switch (btree->type) {
	case BTREE_COL_FIX:
	case BTREE_COL_VAR:
		page->type = WT_PAGE_COL_INT;
		WT_RET(__wt_calloc_def(session, 1, &page->u.col_int.t));
		page->u.col_int.t->recno = 1;
		ref = &page->u.col_int.t->ref;
		page->u.col_int.recno = 1;
		break;
	case BTREE_ROW:
		page->type = WT_PAGE_ROW_INT;
		WT_RET(__wt_calloc_def(session, 1, &page->u.row_int.t));
		ref = &page->u.row_int.t->ref;
		WT_RET(__wt_row_ikey_alloc(session, 0, "", 1,
		    (WT_IKEY **)&((WT_ROW_REF *)ref)->key));
		break;
	WT_ILLEGAL_VALUE(session);
	}
	page->entries = 1;
	page->parent = NULL;
	page->parent_ref = &btree->root_page;
	F_SET(page, WT_PAGE_PINNED);

	btree->root_page.state = WT_REF_MEM;
	btree->root_page.addr = WT_ADDR_INVALID;
	btree->root_page.size = 0;
	btree->root_page.page = page;

	/*
	 * Create a leaf page -- this can be reconciled while the root stays
	 * pinned.
	 */
	WT_RET(__wt_calloc_def(session, 1, &page));
	switch (btree->type) {
	case BTREE_COL_FIX:
		page->u.col_leaf.recno = 1;
		page->type = WT_PAGE_COL_FIX;
		break;
	case BTREE_COL_VAR:
		page->u.col_leaf.recno = 1;
		page->type = WT_PAGE_COL_VAR;
		break;
	case BTREE_ROW:
		page->type = WT_PAGE_ROW_LEAF;
		break;
	}
	page->entries = 0;
	page->parent = btree->root_page.page;
	page->parent_ref = ref;

	ref->state = WT_REF_MEM;
	ref->addr = WT_ADDR_INVALID;
	ref->size = 0;
	ref->page = page;
	return (0);
}

/*
 * __wt_btree_root_empty --
 *	Bulk loads only work on empty trees: check before doing a bulk load.
 */
int
__wt_btree_root_empty(WT_SESSION_IMPL *session, WT_PAGE **leafp)
{
	WT_BTREE *btree;
	WT_PAGE *root, *child;

	btree = session->btree;
	root = btree->root_page.page;

	if (root->entries != 1)
		return (WT_ERROR);
	switch (root->type) {
	case WT_PAGE_COL_INT:
		child = root->u.col_int.t->ref.page;
		break;
	case WT_PAGE_ROW_INT:
		child = root->u.row_int.t->ref.page;
		break;
	WT_ILLEGAL_VALUE(session);
	}
	if (child->entries != 0)
		return (WT_ERROR);

	*leafp = child;
	return (0);
}

/*
 * __btree_last --
 *      Read and pin the last page of the file.
 */
static int
__btree_last(WT_SESSION_IMPL *session)
{
	WT_BTREE *btree;
	WT_PAGE *page;

	btree = session->btree;

	if (btree->type == BTREE_ROW)
		return (0);

	page = NULL;
	WT_RET(__wt_tree_np(session, &page, 0, 0));
	if (page == NULL)
		return (WT_NOTFOUND);

	btree->last_page = page;
	btree->last_recno = __col_last_recno(page);

	/*
	 * If the page is already pinned (that is, the last page is the root
	 * page), we're done, otherwise, pin the last page into memory.
	 */
	if (!F_ISSET(page, WT_PAGE_PINNED)) {
		F_SET(page, WT_PAGE_PINNED);

		/*
		 * Publish: there must be a barrier to ensure the pinned flag
		 * is set before we discard our hazard reference.
		 */
		WT_WRITE_BARRIER();
		__wt_hazard_clear(session, page);
	}
	return (0);
}

/*
 * __btree_close_cache --
 *	Close an in-memory cache of a btree.
 */
static int
__btree_close_cache(WT_SESSION_IMPL *session)
{
	WT_BTREE *btree;
	int ret;

	btree = session->btree;
	ret = 0;

	/*
	 * If it's a normal tree, ask the eviction thread to flush any pages
	 * that remain in the cache.
	 */
	if (!F_ISSET(btree, WT_BTREE_NO_EVICTION))
		WT_TRET(__wt_evict_file_serial(session, 1));

	/*
	 * Write out the free list.
	 * Update the file's description.
	 */
	WT_TRET(__wt_block_freelist_write(session));
	WT_TRET(__wt_desc_update(session));

	return (ret);
}

/*
 * __btree_read_meta --
 *	Read the metadata for a tree.
 */
static int
__btree_read_meta(WT_SESSION_IMPL *session, const char *cfg[], uint32_t flags)
{
	WT_BTREE *btree;
	WT_CONFIG_ITEM cval;

	btree = session->btree;

	btree->flags = flags;
	F_SET(session->btree, WT_BTREE_OPEN);

	/*
	 * Read the file's metadata (unless it's a salvage operation and the
	 * force flag is set, in which case we don't care what the file looks
	 * like).
	 */
	if (LF_ISSET(WT_BTREE_SALVAGE))
		WT_RET(__wt_config_gets(session, cfg, "force", &cval));
	if (!LF_ISSET(WT_BTREE_SALVAGE) || cval.val == 0)
		WT_RET(__wt_desc_read(
		    session, LF_ISSET(WT_BTREE_SALVAGE) ? 1 : 0));

	/* If this is an open for a salvage operation, that's all we do. */
	if (LF_ISSET(WT_BTREE_SALVAGE))
		return (0);

	/* Read in the free list. */
	WT_RET(__wt_block_freelist_read(session));

	/*
	 * Get a root page.  If there's a root page in the file, read it in and
	 * pin it.  If there's no root page, create an empty in-memory page.
	 */
	if (btree->root_page.addr == WT_ADDR_INVALID)
		WT_RET(__wt_btree_root_empty_init(session));
	else  {
		/* If an open for a verify operation, check the disk image. */
		WT_RET(__wt_page_in(session, NULL,
		    &btree->root_page, LF_ISSET(WT_BTREE_VERIFY) ? 1 : 0));
		F_SET(btree->root_page.page, WT_PAGE_PINNED);

		/*
		 * Publish: there must be a barrier to ensure the pinned flag
		 * is set before we discard our hazard reference.
		 */
		WT_WRITE_BARRIER();
		__wt_hazard_clear(session, btree->root_page.page);
	}

	/* Get the last page of the file. */
	WT_RET(__btree_last(session));

	return (0);
}

/*
 * __wt_btree_close --
 *	Close a Btree.
 */
int
__wt_btree_close(WT_SESSION_IMPL *session)
{
	WT_BTREE *btree;
	WT_CONNECTION_IMPL *conn;
	int inuse, ret;

	btree = session->btree;
	conn = S2C(session);
	ret = 0;

	/* Remove from the connection's list. */
	__wt_spin_lock(session, &conn->spinlock);
	inuse = (--btree->refcnt > 0);
	if (!inuse) {
		TAILQ_REMOVE(&conn->btqh, btree, q);
		--conn->btqcnt;
	}
	__wt_spin_unlock(session, &conn->spinlock);
	if (inuse)
		return (0);

	if (F_ISSET(btree, WT_BTREE_OPEN)) {
		WT_TRET(__btree_close_cache(session));
		WT_TRET(__wt_close(session, btree->fh));

		WT_STAT_DECR(conn->stats, file_open);
	}

	/* Free allocated memory. */
	__wt_free(session, btree->name);
	__wt_free(session, btree->filename);
	__wt_free(session, btree->config);
	__wt_free(session, btree->key_format);
	__wt_free(session, btree->key_plan);
	__wt_free(session, btree->idxkey_format);
	__wt_free(session, btree->value_format);
	__wt_free(session, btree->value_plan);
	__wt_btree_huffman_close(session);
	__wt_rec_destroy(session);
	__wt_free(session, btree->stats);

	WT_TRET(__wt_rwlock_destroy(session, btree->rwlock));
	__wt_free(session, session->btree);

	return (ret);
}

/*
 * __wt_btree_reopen --
 *	Reset an open btree handle back to its initial state.
 */
int
__wt_btree_reopen(WT_SESSION_IMPL *session, const char *cfg[], uint32_t flags)
{
	/* Close the existing cache and re-read the metadata. */
	WT_RET(__btree_close_cache(session));
	WT_RET(__btree_read_meta(session, cfg, flags));

	return (0);
}

/*
 * __btree_page_sizes --
 *	Verify the page sizes.
 */
static int
__btree_page_sizes(WT_SESSION_IMPL *session, const char *config)
{
	WT_BTREE *btree;
	WT_CONFIG_ITEM cval;
	uint32_t intl_split_size, leaf_split_size, split_pct;

	btree = session->btree;

	WT_RET(__wt_config_getones(session, config, "allocation_size", &cval));
	btree->allocsize = (uint32_t)cval.val;
	WT_RET(__wt_config_getones(
	    session, config, "internal_page_max", &cval));
	btree->maxintlpage = (uint32_t)cval.val;
	WT_RET(__wt_config_getones(
	    session, config, "internal_item_max", &cval));
	btree->maxintlitem = (uint32_t)cval.val;
	WT_RET(__wt_config_getones(session, config, "leaf_page_max", &cval));
	btree->maxleafpage = (uint32_t)cval.val;
	WT_RET(__wt_config_getones(
	    session, config, "leaf_item_max", &cval));
	btree->maxleafitem = (uint32_t)cval.val;

	/*
	 * Limit allocation units to 128MB, and page sizes to 512MB.  There's
	 * no reason we couldn't support larger sizes (any sizes up to the
	 * smaller of an off_t and a size_t should work), but an application
	 * specifying larger allocation or page sizes would likely be making
	 * as mistake.  The API checked this, but we assert it anyway.
	 */
	WT_ASSERT(session, btree->allocsize >= WT_BTREE_ALLOCATION_SIZE_MIN);
	WT_ASSERT(session, btree->allocsize <= WT_BTREE_ALLOCATION_SIZE_MAX);
	WT_ASSERT(session, btree->maxintlpage <= WT_BTREE_PAGE_SIZE_MAX);
	WT_ASSERT(session, btree->maxleafpage <= WT_BTREE_PAGE_SIZE_MAX);

	/* Allocation sizes must be a power-of-two, nothing else makes sense. */
	if (!__wt_ispo2(btree->allocsize)) {
		__wt_errx(
		    session, "the allocation size must be a power of two");
		return (WT_ERROR);
	}

	/* All page sizes must be in units of the allocation size. */
	if (btree->maxintlpage < btree->allocsize ||
	    btree->maxintlpage % btree->allocsize != 0 ||
	    btree->maxleafpage < btree->allocsize ||
	    btree->maxleafpage % btree->allocsize != 0) {
		__wt_errx(session,
		    "page sizes must be a multiple of the page allocation "
		    "size (%" PRIu32 "B)", btree->allocsize);
		return (WT_ERROR);
	}

	/*
	 * Set the split percentage: reconciliation splits to a
	 * smaller-than-maximum page size so we don't split every time a new
	 * entry is added.
	 */
	WT_RET(__wt_config_getones(session, config, "split_pct", &cval));
	split_pct = (uint32_t)cval.val;
	intl_split_size = WT_SPLIT_PAGE_SIZE(
	    btree->maxintlpage, btree->allocsize, split_pct);
	leaf_split_size = WT_SPLIT_PAGE_SIZE(
	    btree->maxleafpage, btree->allocsize, split_pct);

	/*
	 * Default values for internal and leaf page items: make sure at least
	 * 8 items fit on split pages.
	 */
	if (btree->maxintlitem == 0)
		    btree->maxintlitem = intl_split_size / 8;
	if (btree->maxleafitem == 0)
		    btree->maxleafitem = leaf_split_size / 8;

	/*
	 * The API limits the minimum overflow size, but just in case: we'd fail
	 * horribly if the overflow limit was smaller than an overflow chunk.
	 */
	WT_ASSERT(session, btree->maxintlitem > sizeof(WT_OFF) + 10);
	WT_ASSERT(session, btree->maxleafitem > sizeof(WT_OFF) + 10);

	/* Check we can fit at least 2 items on a page. */
	if (btree->maxintlitem > btree->maxintlpage / 2)
		return (pse1(session, "internal",
		    btree->maxintlpage, btree->maxintlitem));
	if (btree->maxleafitem > btree->maxleafpage / 2)
		return (pse1(session, "leaf",
		    btree->maxleafpage, btree->maxleafitem));

	/*
	 * Take into account the size of a split page:
	 *
	 * Make it a separate error message so it's clear what went wrong.
	 */
	if (btree->maxintlitem > intl_split_size / 2)
		return (pse2(session, "internal",
		    btree->maxintlpage, btree->maxintlitem, split_pct));
	if (btree->maxleafitem > leaf_split_size / 2)
		return (pse2(session, "leaf",
		    btree->maxleafpage, btree->maxleafitem, split_pct));

	return (0);
}

static int
pse1(WT_SESSION_IMPL *session, const char *type, uint32_t max, uint32_t ovfl)
{
	__wt_errx(session,
	    "%s page size (%" PRIu32 "B) too small for the maximum item size "
	    "(%" PRIu32 "B); the page must be able to hold at least 2 items",
	    type, max, ovfl);
	return (WT_ERROR);
}

static int
pse2(WT_SESSION_IMPL *session,
    const char *type, uint32_t max, uint32_t ovfl, uint32_t pct)
{
	__wt_errx(session,
	    "%s page size (%" PRIu32 "B) too small for the maximum item size "
	    "(%" PRIu32 "B), because of the split percentage (%" PRIu32
	    "%%); a split page must be able to hold at least 2 items",
	    type, max, ovfl, pct);
	return (WT_ERROR);
}
