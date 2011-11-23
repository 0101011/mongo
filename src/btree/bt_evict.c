/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

static void __evict_dup_remove(WT_SESSION_IMPL *);
static int  __evict_file(WT_SESSION_IMPL *, WT_EVICT_REQ *);
static int  __evict_lru(WT_SESSION_IMPL *);
static int  __evict_lru_cmp(const void *, const void *);
static void __evict_page(WT_SESSION_IMPL *);
static int  __evict_page_cmp(const void *, const void *);
static int  __evict_request_retry(WT_SESSION_IMPL *);
static int  __evict_request_walk(WT_SESSION_IMPL *);
static int  __evict_walk(WT_SESSION_IMPL *);
static int  __evict_walk_file(WT_SESSION_IMPL *, u_int *);
static int  __evict_worker(WT_SESSION_IMPL *);

/*
 * Tuning constants: I hesitate to call this tuning, but we want to review some
 * number of pages from each file's in-memory tree for each page we evict.
 */
#define	WT_EVICT_GROUP		50	/* Evict N pages at a time */
#define	WT_EVICT_WALK_PER_TABLE	20	/* Pages to visit per file */
#define	WT_EVICT_WALK_BASE	100	/* Pages tracked across file visits */

/*
 * WT_EVICT_FOREACH --
 *	Walk a list of eviction candidates.
 */
#define	WT_EVICT_FOREACH(cache, p, i)					\
	for ((i) = 0, (p) = (cache)->evict; (i) < WT_EVICT_GROUP; ++(i), ++(p))

/*
 * WT_EVICT_REQ_FOREACH --
 *	Walk a list of eviction requests.
 */
#define	WT_EVICT_REQ_FOREACH(er, er_end, cache)				\
	for ((er) = (cache)->evict_request,				\
	    (er_end) = (er) + WT_ELEMENTS((cache)->evict_request);	\
	    (er) < (er_end); ++(er))

/*
 * __evict_clr --
 *	Clear an entry in the eviction list.
 */
static inline void
__evict_clr(WT_EVICT_LIST *e)
{
	e->page = NULL;
	e->btree = WT_DEBUG_POINT;
}

/*
 * __evict_req_set --
 *	Set an entry in the eviction request list.
 */
static inline void
__evict_req_set(
    WT_SESSION_IMPL *session, WT_EVICT_REQ *r, WT_PAGE *page, uint32_t flags)
{
					/* Should be empty */
	WT_ASSERT(session, r->session == NULL);

	WT_CLEAR(*r);
	r->btree = session->btree;
	r->page = page;
	r->flags = flags;

	/*
	 * Publish: there must be a barrier to ensure the structure fields are
	 * set before the eviction thread can see the request.
	 */
	WT_PUBLISH(r->session, session);
}

/*
 * __evict_req_clr --
 *	Clear an entry in the eviction request list.
 */
static inline void
__evict_req_clr(WT_SESSION_IMPL *session, WT_EVICT_REQ *r)
{
	__wt_free(session, r->retry);

	/*
	 * Publish; there must be a barrier to ensure the structure fields are
	 * set before the entry is made available for re-use.
	 */
	WT_PUBLISH(r->session, NULL);
}

/*
 * __wt_evict_server_wake --
 *	See if the eviction server thread needs to be awakened.
 */
void
__wt_evict_server_wake(WT_CONNECTION_IMPL *conn, int force)
{
	WT_CACHE *cache;
	WT_SESSION_IMPL *session;
	uint64_t bytes_inuse, bytes_max;

	cache = conn->cache;
	session = &conn->default_session;

	/*
	 * If we're locking out reads, or within 95% of our cache limit, or
	 * forcing the issue (when closing the environment), run the eviction
	 * server.
	 */
	bytes_inuse = __wt_cache_bytes_inuse(cache);
	bytes_max = conn->cache_size;
	if (!force && !cache->read_lockout &&
	    bytes_inuse < bytes_max - bytes_max / 20)
		return;

	WT_VERBOSE(session, EVICTSERVER,
	    "waking eviction server: force %sset, read lockout %sset, "
	    "bytes inuse %s max (%" PRIu64 "MB %s %" PRIu64 "MB), ",
	    force ? "" : "not ", cache->read_lockout ? "" : "not ",
	    bytes_inuse <= bytes_max ? "<=" : ">",
	    bytes_inuse / WT_MEGABYTE,
	    bytes_inuse <= bytes_max ? "<=" : ">",
	    bytes_max / WT_MEGABYTE);

	__wt_cond_signal(session, cache->evict_cond);
}

/*
 * __evict_file_serial_func --
 *	Eviction serialization function called when a tree is being flushed
 *	or closed.
 */
void
__wt_evict_file_serial_func(WT_SESSION_IMPL *session)
{
	WT_CACHE *cache;
	WT_EVICT_REQ *er, *er_end;
	int close_method;

	__wt_evict_file_unpack(session, &close_method);

	cache = S2C(session)->cache;

	/* Find an empty slot and enter the eviction request. */
	WT_EVICT_REQ_FOREACH(er, er_end, cache)
		if (er->session == NULL) {
			__evict_req_set(session, er, NULL, close_method ?
			    WT_EVICT_REQ_CLOSE : 0);
			return;
		}

	__wt_errx(session, "eviction server request table full");
	__wt_session_serialize_wrapup(session, NULL, WT_ERROR);
}

/*
 * __evict_page_serial_func --
 *	Eviction serialization function called when a page needs to be forced
 *	out due to the volume of inserts.
 */
void
__wt_evict_page_serial_func(WT_SESSION_IMPL *session)
{
	WT_CACHE *cache;
	WT_EVICT_REQ *er, *er_end;
	WT_PAGE *page;

	__wt_evict_page_unpack(session, &page);

	cache = S2C(session)->cache;

	/* Find an empty slot and enter the eviction request. */
	WT_EVICT_REQ_FOREACH(er, er_end, cache)
		if (er->session == NULL) {
			__evict_req_set(session, er, page, WT_EVICT_REQ_PAGE);
			return;
		}

	__wt_errx(session, "eviction server request table full");
	__wt_session_serialize_wrapup(session, NULL, WT_ERROR);
}

/*
 * __wt_evict_force_clear
 *	Clear the force flag from a page and clear any pending requests to
 *	evict the page.
 */
void
__wt_evict_force_clear(WT_SESSION_IMPL *session, WT_PAGE *page)
{
	WT_CACHE *cache;
	WT_EVICT_LIST *evict;
	WT_EVICT_REQ *er, *er_end;
	u_int i;

	cache = S2C(session)->cache;

	/*
	 * If we evict a page marked for forced eviction, clear any reference
	 * to it from the request queue or the list of pages tracked for
	 * eviction.
	 */
	WT_EVICT_REQ_FOREACH(er, er_end, cache)
		if (er->session != NULL &&
		    F_ISSET(er, WT_EVICT_REQ_PAGE) &&
		    er->page == page)
			__evict_req_clr(session, er);

	if (cache->evict != NULL)
		WT_EVICT_FOREACH(cache, evict, i)
			if (evict->page == page)
				__evict_clr(evict);

	F_CLR(page, WT_PAGE_FORCE_EVICT);
}

/*
 * __wt_cache_evict_server --
 *	Thread to evict pages from the cache.
 */
void *
__wt_cache_evict_server(void *arg)
{
	WT_CONNECTION_IMPL *conn;
	WT_SESSION_IMPL *session;
	WT_CACHE *cache;
	WT_EVICT_REQ *er, *er_end;
	int ret;

	conn = arg;
	cache = conn->cache;
	ret = 0;

	/*
	 * We need a session handle because we're reading/writing pages.
	 * Start with the default session to keep error handling simple.
	 */
	session = &conn->default_session;
	WT_ERR(__wt_open_session(conn, 1, NULL, NULL, &session));

	while (F_ISSET(conn, WT_SERVER_RUN)) {
		WT_VERBOSE(session, EVICTSERVER, "eviction server sleeping");
		__wt_cond_wait(session, cache->evict_cond);
		if (!F_ISSET(conn, WT_SERVER_RUN))
			break;
		WT_VERBOSE(session, EVICTSERVER, "eviction server waking");

		/* Evict pages from the cache as needed. */
		WT_ERR(__evict_worker(session));
	}

	if (ret == 0) {
		if (__wt_cache_bytes_inuse(cache) != 0) {
			__wt_errx(session,
			    "cache server: exiting with %" PRIu64 " pages, "
			    "%" PRIu64 " bytes in use",
			    __wt_cache_pages_inuse(cache),
			    __wt_cache_bytes_inuse(cache));
		}
	} else
err:		__wt_err(session, ret, "cache eviction server error");

	WT_VERBOSE(session, EVICTSERVER, "cache eviction server exiting");

	__wt_free(session, cache->evict);
	WT_EVICT_REQ_FOREACH(er, er_end, cache)
		__wt_free(session, er->retry);

	if (session != &conn->default_session)
		(void)session->iface.close(&session->iface, NULL);

	return (NULL);
}

/*
 * __evict_worker --
 *	Evict pages from memory.
 */
static int
__evict_worker(WT_SESSION_IMPL *session)
{
	WT_CACHE *cache;
	WT_CONNECTION_IMPL *conn;
	uint64_t bytes_start, bytes_inuse, bytes_max;
	int loop;

	conn = S2C(session);
	cache = conn->cache;

	/* Evict pages from the cache. */
	for (loop = 0;; loop++) {
		/* Walk the eviction-request queue. */
		WT_RET(__evict_request_walk(session));

		/*
		 * Eviction requests can temporarily fail when a tree is active,
		 * that is, we may not be able to immediately reconcile all of
		 * the file's pages.  If the pending_retry value is non-zero, it
		 * means there are pending requests we need to handle.
		 *
		 * Do general eviction even if we're just handling pending retry
		 * requests.  The problematic case is when reads are locked out
		 * because we're out of memory in the cache, a reading thread
		 * is blocked, and that thread has a hazard reference blocking
		 * us from reconciling a page that's part of a pending request.
		 * Keep pushing out blocks from the general pool as well as the
		 * pending requests until the system unjams.
		 */
		while (cache->pending_retry) {
			WT_RET(__evict_request_retry(session));
			if (cache->pending_retry)
				WT_RET(__evict_lru(session));
		}

		/*
		 * Keep evicting until we hit 90% of the maximum cache size.
		 */
		bytes_inuse = __wt_cache_bytes_inuse(cache);
		bytes_max = conn->cache_size;
		/* If reads are locked out, ping the read server. */
		if (cache->read_lockout && bytes_inuse < bytes_max)
			__wt_read_server_wake(conn, 1);
		if (bytes_inuse < bytes_max - (bytes_max / 10))
			break;

		WT_RET(__evict_lru(session));

		/*
		 * If we're making progress, keep going; if we're not making
		 * any progress at all, go back to sleep, it's not something
		 * we can fix.
		 */
		bytes_start = bytes_inuse;
		bytes_inuse = __wt_cache_bytes_inuse(cache);
		if (bytes_start == bytes_inuse) {
			if (loop == 10) {
				WT_STAT_INCR(conn->stats, cache_evict_slow);
				WT_VERBOSE(session, EVICTSERVER,
				    "eviction server: "
				    "unable to reach eviction goal");
				break;
			}
		} else
			loop = 0;
	}
	return (0);
}

/*
 * __evict_request_walk --
 *	Walk the eviction request queue.
 */
static int
__evict_request_walk(WT_SESSION_IMPL *session)
{
	WT_SESSION_IMPL *request_session;
	WT_CACHE *cache;
	WT_EVICT_REQ *er, *er_end;
	int ret;

	cache = S2C(session)->cache;

	/*
	 * Walk the eviction request queue, looking for sync or close requests
	 * (defined by a valid WT_SESSION_IMPL handle).  If we find a request,
	 * perform it, flush the result and clear the request slot, then wake
	 * up the requesting thread.
	 */
	WT_EVICT_REQ_FOREACH(er, er_end, cache) {
		if ((request_session = er->session) == NULL)
			continue;

		/*
		 * The eviction candidate list might reference pages we are
		 * about to discard; clear it.
		 */
		memset(cache->evict, 0, cache->evict_allocated);

		/* Reference the correct WT_BTREE handle. */
		WT_SET_BTREE_IN_SESSION(session, er->btree);

		if (F_ISSET(er, WT_EVICT_REQ_PAGE))
			ret = __wt_page_reconcile(session,
			    er->page, WT_REC_WAIT);
		else
			ret = __evict_file(session, er);

		/* Clear the reference to the btree handle. */
		WT_CLEAR_BTREE_IN_SESSION(session);

		/*
		 * If we don't have any pages to retry, we're done, resolve the
		 * request.  If we have pages to retry, we have to wait for the
		 * main eviction loop to finish the work.
		 */
		if (er->retry == NULL) {
			/*
			 * XXX Page eviction is special: the requesting thread
			 * is already inside wrapup.
			 */
			if (!F_ISSET(er, WT_EVICT_REQ_PAGE))
				__wt_session_serialize_wrapup(
				    request_session, NULL, ret);
			__evict_req_clr(session, er);
		} else
			cache->pending_retry = 1;
	}
	return (0);
}

/*
 * __evict_file --
 *	Flush pages for a specific file as part of a close/sync operation.
 */
static int
__evict_file(WT_SESSION_IMPL *session, WT_EVICT_REQ *er)
{
	WT_BTREE *btree;
	WT_PAGE *next_page, *page;
	uint32_t flags;

	btree = session->btree;
	flags = F_ISSET(er, WT_EVICT_REQ_CLOSE) ?
	    WT_REC_EVICT | WT_REC_LOCKED : 0;

	WT_VERBOSE(session, EVICTSERVER,
	    "eviction: %s file request: %s",
	    btree->name, F_ISSET(er, WT_EVICT_REQ_CLOSE) ? "close" : "sync");

	/*
	 * Discard any page we're holding: we're about to do a walk of the file
	 * tree, and if we're closing the file, there won't be pages to evict
	 * in the future, that is, our location in the tree is no longer useful.
	 */
	btree->evict_page = NULL;

	/*
	 * We can't evict the page just returned to us, it marks our place in
	 * the tree.  So, always stay one page ahead of the page being returned.
	 */
	next_page = NULL;
	WT_RET(__wt_tree_np(session, &next_page, 1, 1));
	for (;;) {
		if ((page = next_page) == NULL)
			break;
		WT_RET(__wt_tree_np(session, &next_page, 1, 1));

		/*
		 * Sync: only dirty pages need reconciliation, and we ignore
		 * pinned pages because we can't assure total access to them
		 * (hazard references don't apply).
		 *
		 * Close: discarding all of the file's pages from the cache,
		 * and reconciliation is how we do that.
		 */
		if (!F_ISSET(er, WT_EVICT_REQ_CLOSE) &&
		    (!WT_PAGE_IS_MODIFIED(page) ||
		    F_ISSET(page, WT_PAGE_PINNED)))
			continue;
		if (__wt_page_reconcile(session, page, flags) == 0)
			continue;

		/*
		 * We weren't able to reconcile the page: possible in sync if
		 * another thread of control holds a hazard reference on the
		 * page we're reconciling (or a hazard reference on a deleted
		 * or split page in that page's subtree), is trying to read
		 * another page, and can't as the read subsystem is locked out
		 * right now.  Possible in close or sync if the file system is
		 * full.
		 *
		 * Add this page to the list of pages we'll have to retry.
		 */
		if (er->retry_next == er->retry_entries) {
			WT_RET(__wt_realloc(session, &er->retry_allocated,
			    (er->retry_entries + 100) *
			    sizeof(*er->retry), &er->retry));
			er->retry_entries += 100;
		}
		er->retry[er->retry_next++] = page;
	}

	return (0);
}

/*
 * __evict_request_retry --
 *	Retry an eviction request.
 */
static int
__evict_request_retry(WT_SESSION_IMPL *session)
{
	WT_CACHE *cache;
	WT_EVICT_REQ *er, *er_end;
	WT_PAGE *page;
	WT_SESSION_IMPL *request_session;
	uint32_t i, flags;
	int pending_retry;

	cache = S2C(session)->cache;

	/* Reset the flag for pending retry requests. */
	cache->pending_retry = 0;

	/*
	 * Walk the eviction request queue, looking for sync or close requests
	 * that need to be retried (defined by a non-NULL retry reference).
	 */
	WT_EVICT_REQ_FOREACH(er, er_end, cache) {
		if (er->retry == NULL)
			continue;

		/*
		 * The eviction candidate list might reference pages we are
		 * about to discard; clear it.
		 */
		memset(cache->evict, 0, cache->evict_allocated);

		/* Reference the correct WT_BTREE handle. */
		request_session = er->session;
		WT_SET_BTREE_IN_SESSION(session, request_session->btree);
		WT_VERBOSE(session, EVICTSERVER,
		    "eviction: %s file request retry: %s",
		    request_session->btree->name,
		    F_ISSET(er, WT_EVICT_REQ_CLOSE) ? "close" : "sync");

		/*
		 * Set the reconcile flags: should never be close, but we
		 * can do the work even if it is.
		 */
		flags = F_ISSET(er, WT_EVICT_REQ_CLOSE) ?
		    WT_REC_EVICT | WT_REC_LOCKED : 0;

		/* Walk the list of retry requests. */
		for (pending_retry = 0, i = 0; i < er->retry_entries; ++i) {
			if ((page = er->retry[i]) == NULL)
				continue;
			if (__wt_page_reconcile(session, page, flags) == 0)
				er->retry[i] = NULL;
			else
				pending_retry = 1;
		}

		/*
		 * If we finished, clean up and resolve the request, otherwise
		 * there's still work to do.
		 */
		if (pending_retry && ++er->retry_cnt < 5)
			cache->pending_retry = 1;
		else {
			__wt_session_serialize_wrapup(
			    request_session, NULL, pending_retry ? EBUSY : 0);
			__evict_req_clr(session, er);
		}

		WT_CLEAR_BTREE_IN_SESSION(session);
	}
	return (0);
}

/*
 * __evict_lru --
 *	Evict pages from the cache based on their read generation.
 */
static int
__evict_lru(WT_SESSION_IMPL *session)
{
	/* Get some more pages to consider for eviction. */
	WT_RET(__evict_walk(session));

	/* Remove duplicates from the list. */
	__evict_dup_remove(session);

	/* Reconcile and discard the pages. */
	__evict_page(session);

	return (0);
}

/*
 * __evict_walk --
 *	Fill in the array by walking the next set of pages.
 */
static int
__evict_walk(WT_SESSION_IMPL *session)
{
	WT_CONNECTION_IMPL *conn;
	WT_BTREE *btree;
	WT_CACHE *cache;
	u_int elem, i;
	int ret;

	conn = S2C(session);
	cache = S2C(session)->cache;

	/*
	 * Resize the array in which we're tracking pages, as necessary, then
	 * get some pages from each underlying file.  We hold a mutex for the
	 * entire time -- it's slow, but (1) how often do new files get added
	 * or removed to/from the system, and (2) it's all in-memory stuff, so
	 * it's not that slow.
	 */
	ret = 0;
	__wt_spin_lock(session, &conn->spinlock);

	elem = WT_EVICT_WALK_BASE + (conn->btqcnt * WT_EVICT_WALK_PER_TABLE);
	if (elem > cache->evict_entries) {
		WT_ERR(__wt_realloc(session, &cache->evict_allocated,
		    elem * sizeof(WT_EVICT_LIST), &cache->evict));
		cache->evict_entries = elem;
	}

	i = WT_EVICT_WALK_BASE;
	TAILQ_FOREACH(btree, &conn->btqh, q) {
		/* Skip trees we're not allowed to touch. */
		if (F_ISSET(btree, WT_BTREE_NO_EVICTION))
			continue;

		/* Reference the correct WT_BTREE handle. */
		WT_SET_BTREE_IN_SESSION(session, btree);

		ret = __evict_walk_file(session, &i);

		WT_CLEAR_BTREE_IN_SESSION(session);

		if (ret != 0)
			goto err;
	}

err:	__wt_spin_unlock(session, &conn->spinlock);
	return (ret);
}

/*
 * __evict_walk_file --
 *	Get a few page eviction candidates from a single underlying file.
 */
static int
__evict_walk_file(WT_SESSION_IMPL *session, u_int *slotp)
{
	WT_BTREE *btree;
	WT_CACHE *cache;
	WT_PAGE *page;
	int i, restarted_once;

	btree = session->btree;
	cache = S2C(session)->cache;

	/*
	 * Get the next WT_EVICT_WALK_PER_TABLE entries.
	 *
	 * We can't evict the page just returned to us, it marks our place in
	 * the tree.  So, always stay one page ahead of the page being returned.
	 */
	i = restarted_once = 0;
	do {
		/*
		 * Pinned pages can't be evicted, and it's not useful to try
		 * and evict deleted or temporary pages.
		 */
		page = btree->evict_page;
		if (page != NULL && !F_ISSET(page,
		    WT_PAGE_PINNED | WT_PAGE_DELETED | WT_PAGE_MERGE)) {
			WT_VERBOSE(session, EVICTSERVER,
			    "eviction: %s walk: %" PRIu32,
			    btree->name, WT_PADDR(page));

			++i;
			cache->evict[*slotp].page = page;
			cache->evict[*slotp].btree = btree;
			++*slotp;
		}

		WT_RET(__wt_tree_np(session, &btree->evict_page, 1, 1));
		if (btree->evict_page == NULL && restarted_once++ == 1)
			break;
	} while (i < WT_EVICT_WALK_PER_TABLE);

	return (0);
}

/*
 * __evict_dup_remove --
 *	Discard duplicates from the list of pages we collected.
 */
static void
__evict_dup_remove(WT_SESSION_IMPL *session)
{
	WT_CACHE *cache;
	WT_EVICT_LIST *evict;
	u_int elem, i, j;

	cache = S2C(session)->cache;

	/*
	 * We have an array of page eviction references that may contain NULLs,
	 * as well as duplicate entries.
	 *
	 * First, sort the array by WT_REF address, then delete any duplicates.
	 * The reason is because we might evict the page but leave a duplicate
	 * entry in the "saved" area of the array, and that would be a NULL
	 * dereference on the next run.  (If someone ever tries to remove this
	 * duplicate cleanup for better performance, you can't fix it just by
	 * checking the WT_REF state -- that only works if you are discarding
	 * a page from a single level of the tree; if you are discarding a
	 * page and its parent, the duplicate of the page's WT_REF might have
	 * been free'd before a subsequent review of the eviction array.)
	 */
	evict = cache->evict;
	elem = cache->evict_entries;
	qsort(evict,
	    (size_t)elem, sizeof(WT_EVICT_LIST), __evict_page_cmp);
	for (i = 0; i < elem; i = j)
		for (j = i + 1; j < elem; ++j) {
			/*
			 * If the leading pointer hits a NULL, we're done, the
			 * NULLs all sorted to the top of the array.
			 */
			if (evict[j].page == NULL)
				return;

			/* Delete the second and any subsequent duplicates. */
			if (evict[i].page == evict[j].page)
				__evict_clr(&evict[j]);
			else
				break;
		}
}

/*
 * __evict_page --
 *	Reconcile and discard cache pages.
 */
static void
__evict_page(WT_SESSION_IMPL *session)
{
	WT_CACHE *cache;
	WT_EVICT_LIST *evict;
	WT_PAGE *page;
	u_int i;

	cache = S2C(session)->cache;

	/* Sort the array by LRU, then evict the most promising candidates. */
	qsort(cache->evict, (size_t)cache->evict_entries,
	    sizeof(WT_EVICT_LIST), __evict_lru_cmp);

	WT_EVICT_FOREACH(cache, evict, i) {
		/*
		 * NULL pages sort to the end of the list: once we hit one,
		 * give up.
		 */
		if ((page = evict->page) == NULL)
			break;

		/* Reference the correct WT_BTREE handle. */
		WT_SET_BTREE_IN_SESSION(session, evict->btree);

		/*
		 * Paranoia: remove the entry so we never try and reconcile
		 * the same page on reconciliation error.
		 */
		__evict_clr(evict);

		/*
		 * For now, we don't care why reconciliation failed -- we expect
		 * the reason is we were unable to get exclusive access for the
		 * page, but it might be we're out of disk space.   Regardless,
		 * try not to pick the same page every time.
		 */
		if (__wt_page_reconcile(session, page, WT_REC_EVICT) != 0)
			page->read_gen = __wt_cache_read_gen(session);

		WT_CLEAR_BTREE_IN_SESSION(session);
	}
}

/*
 * __evict_page_cmp --
 *	Qsort function: sort WT_EVICT_LIST array based on the page's address.
 */
static int
__evict_page_cmp(const void *a, const void *b)
{
	WT_PAGE *a_page, *b_page;

	/*
	 * There may be NULL references in the array; sort them as greater than
	 * anything else so they migrate to the end of the array.
	 */
	a_page = ((WT_EVICT_LIST *)a)->page;
	b_page = ((WT_EVICT_LIST *)b)->page;
	if (a_page == NULL)
		return (b_page == NULL ? 0 : 1);
	if (b_page == NULL)
		return (-1);

	/* Sort the page address in ascending order. */
	return (a_page > b_page ? 1 : (a_page < b_page ? -1 : 0));
}

/*
 * __evict_lru_cmp --
 *	Qsort function: sort WT_EVICT_LIST array based on the page's read
 *	generation.
 */
static int
__evict_lru_cmp(const void *a, const void *b)
{
	WT_PAGE *a_page, *b_page;
	uint64_t a_lru, b_lru;

	/*
	 * There may be NULL references in the array; sort them as greater than
	 * anything else so they migrate to the end of the array.
	 */
	a_page = ((WT_EVICT_LIST *)a)->page;
	b_page = ((WT_EVICT_LIST *)b)->page;
	if (a_page == NULL)
		return (b_page == NULL ? 0 : 1);
	if (b_page == NULL)
		return (-1);

	/* Sort the LRU in ascending order. */
	a_lru = a_page->read_gen;
	b_lru = b_page->read_gen;
	return (a_lru > b_lru ? 1 : (a_lru < b_lru ? -1 : 0));
}
