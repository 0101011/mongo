/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 *
 * ex_schema.c
 *	This is an example application demonstrating how to create and access
 *	tables using a schema.
 */

#include <stdio.h>
#include <string.h>

#include <inttypes.h>
#include <wiredtiger.h>

const char *home = "WT_TEST";

/* The C struct for the data we are storing with WiredTiger. */
typedef struct {
	char country[5];
	uint16_t year;
	uint64_t population;
} POP_RECORD;

POP_RECORD pop_data[] = {
	{ "USA", 1980, 226542250 },
	{ "USA", 2009, 307006550 },
	{ "UK", 2008, 61414062 },
	{ "CAN", 2008, 33311400 },
	{ "AU", 2008, 21431800 }
};

int main(void)
{
	int ret;
	WT_CONNECTION *conn;
	WT_SESSION *session;
	WT_CURSOR *cursor;
	POP_RECORD *p, *endp;
	const char *country;
	uint64_t recno;
	uint16_t year;

	ret = wiredtiger_open(home, NULL, "create", &conn);
	if (ret != 0)
		fprintf(stderr, "Error connecting to %s: %s\n",
		    home, wiredtiger_strerror(ret));
	/* Note: error checking omitted for clarity. */

	ret = conn->open_session(conn, NULL, NULL, &session);

	/*
	 * Create the population table.
	 * Keys are record numbers, the format for values is
	 * (5-byte string, short, long).
	 * See ::wiredtiger_struct_pack for details of the format strings.
	 *
	 * If this program is run multiple times so the table already exists,
	 * this call will verify that the table exists.  It is not required in
	 * that case, but is a safety check that the schema matches what the
	 * program expects.
	 */
	ret = session->create(session, "table:population",
	    "key_format=r,"
	    "value_format=5sHQ,"
	    "columns=(id,country,year,population),"
	    "colgroups=(main,population)");

	/* Create the column groups to store population in its own file. */
	ret = session->create(session, "colgroup:population:main",
	    "columns=(country,year)");

	ret = session->create(session, "colgroup:population:population",
	    "columns=(population)");

	/* Create an index with composite key (country,year). */
	ret = session->create(session, "index:population:country_year",
	    "columns=(country,year)");

	ret = session->open_cursor(session, "table:population",
	    NULL, "append", &cursor);

	endp = pop_data + (sizeof (pop_data) / sizeof (pop_data[0]));
	for (p = pop_data; p < endp; p++) {
		cursor->set_value(cursor, p->country, p->year, p->population);
		ret = cursor->insert(cursor);
	}
	ret = cursor->close(cursor, NULL);

	/* Now just read through the countries we know about */
	ret = session->open_cursor(session,
	    "index:population:country_year(id)",
	    NULL, NULL, &cursor);

	while ((ret = cursor->next(cursor)) == 0) {
		cursor->get_key(cursor, &country, &year);
		cursor->get_value(cursor, &recno);

		printf("Got country %s : row ID %d\n", country, (int)recno);
	}

	ret = conn->close(conn, NULL);

	return (ret);
}
