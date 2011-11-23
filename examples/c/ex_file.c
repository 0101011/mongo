/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 *
 * ex_file.c
 *	This is an example demonstrating how to configure an individual file.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wiredtiger.h>

const char *home = "WT_TEST";

int
main(void)
{
	WT_CONNECTION *conn;
	WT_SESSION *session;
	int ret;

	if ((ret = wiredtiger_open(home, NULL, "create", &conn)) != 0 ||
	    (ret = conn->open_session(conn, NULL, NULL, &session)) != 0)
		fprintf(stderr, "Error connecting to %s: %s\n",
		    home, wiredtiger_strerror(ret));
	/* Note: further error checking omitted for clarity. */

	ret = session->create(session, "file:example",
	    "key_format=u,"
	    "internal_page_max=32KB,internal_item_max=1KB,"
	    "leaf_page_max=1MB,leaf_item_max=32KB");

	return (conn->close(conn, NULL) == 0 ? ret : EXIT_FAILURE);
}
