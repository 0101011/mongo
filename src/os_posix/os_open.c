/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include "wt_internal.h"

/*
 * __wt_open --
 *	Open a file handle.
 */
int
__wt_open(WT_SESSION_IMPL *session,
    const char *name, mode_t mode, int ok_create, WT_FH **fhp)
{
	WT_CONNECTION_IMPL *conn;
	WT_FH *fh;
	int f, fd, matched, ret;

	conn = S2C(session);
	fh = NULL;

	 WT_VERBOSE(session, FILEOPS, "fileops: %s: open", name);

	/* Increment the reference count if we already have the file open. */
	matched = 0;
	__wt_lock(session, conn->mtx);
	TAILQ_FOREACH(fh, &conn->fhqh, q) {
		if (strcmp(name, fh->name) == 0) {
			++fh->refcnt;
			*fhp = fh;
			matched = 1;
			break;
		}
	}
	__wt_unlock(session, conn->mtx);
	if (matched)
		return (0);

	f = O_RDWR;
#ifdef O_BINARY
	/* Windows clones: we always want to treat the file as a binary. */
	f |= O_BINARY;
#endif
	if (ok_create)
		f |= O_CREAT;

	if ((fd = open(name, f, mode)) == -1) {
		__wt_err(session, errno, "%s", name);
		return (WT_ERROR);
	}

	WT_RET(__wt_calloc(session, 1, sizeof(WT_FH), &fh));
	WT_ERR(__wt_strdup(session, name, &fh->name));

#if defined(HAVE_FCNTL) && defined(FD_CLOEXEC)
	/*
	 * Security:
	 * The application may spawn a new process, and we don't want another
	 * process to have access to our file handles.  There's an obvious
	 * race here...
	 */
	if ((f = fcntl(fd, F_GETFD)) == -1 ||
	    fcntl(fd, F_SETFD, f | FD_CLOEXEC) == -1) {
		__wt_err(session, errno, "%s: fcntl", name);
		goto err;
	}
#endif

	fh->fd = fd;
	fh->refcnt = 1;
	*fhp = fh;

	/* Set the file's size. */
	WT_ERR(__wt_filesize(session, fh, &fh->file_size));

	/* Link onto the environment's list of files. */
	__wt_lock(session, conn->mtx);
	TAILQ_INSERT_TAIL(&conn->fhqh, fh, q);
	__wt_unlock(session, conn->mtx);

	return (0);

err:	if (fh != NULL) {
		if (fh->name != NULL)
			__wt_free(session, fh->name);
		__wt_free(session, fh);
	}
	(void)close(fd);
	return (ret);
}

/*
 * __wt_close --
 *	Close a file handle.
 */
int
__wt_close(WT_SESSION_IMPL *session, WT_FH *fh)
{
	WT_CONNECTION_IMPL *conn;
	int ret;

	conn = S2C(session);
	ret = 0;

	if (fh == NULL || fh->refcnt == 0 || --fh->refcnt > 0)
		return (0);

	/* Remove from the list and discard the memory. */
	__wt_lock(session, conn->mtx);
	TAILQ_REMOVE(&conn->fhqh, fh, q);
	__wt_unlock(session, conn->mtx);

	if (close(fh->fd) != 0) {
		__wt_err(session, errno, "%s", fh->name);
		ret = WT_ERROR;
	}

	__wt_free(session, fh->name);
	__wt_free(session, fh);
	return (ret);
}
