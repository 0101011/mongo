/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008 WiredTiger Software.
 *	All rights reserved.
 *
 * $Id$
 */

#include "wts.h"

char *
fname(const char *prefix, const char *name)
{
	static char buf[128];

	if (prefix == NULL)
		(void)snprintf(buf, sizeof(buf), "__%s", name);
	else
		(void)snprintf(buf, sizeof(buf), "__%s.%s", prefix, name);
	return (buf);
}

void
key_gen(DBT *key, u_int64_t keyno)
{
	static size_t blen;
	static char *buf;
	size_t i;

	/*
	 * The key is a variable length item with a leading 10-digit value.
	 * Since we have to be able re-construct it from the record number
	 * (when doing row lookups), we pre-load a set of random lengths in
	 * a lookup table, and then use the record number to choose one of
	 * the pre-loaded lengths.
	 *
	 * Fill in the random key lengths.
	 */
	if (blen < g.c_key_max) {
		if (buf != NULL) {
			free(buf);
			buf = NULL;
		}
		for (i = 0;
		    i < sizeof(g.key_rand_len) / sizeof(g.key_rand_len[0]); ++i)
			g.key_rand_len[i] = MMRAND(g.c_key_min, g.c_key_max);
		blen = g.c_key_max;
		if ((buf = malloc(blen)) == NULL) {
			fprintf(stderr,
			    "%s: %s\n", g.progname, strerror(errno));
			exit(EXIT_FAILURE);
		}
		for (i = 0; i < blen; ++i)
			buf[i] = 'a' + i % 26;
	}

	/* The key always starts with a 10-digit string (the specified cnt). */
	sprintf(buf, "%010llu", keyno);
	buf[10] = '/';

	key->data = buf;
	key->size = g.key_rand_len[keyno %
	    (sizeof(g.key_rand_len) / sizeof(g.key_rand_len[0]))];
}

void
data_gen(DBT *data)
{
	static size_t blen;
	static u_int r;
	static char *buf;
	size_t i, len;
	char *p;

	/*
	 * Set buffer contents.
	 *
	 * If doing repeat compression, use different data some percentage of
	 * the time, otherwise we end up with a single chunk of repeated data.
	 * Add a few extra bytes in order to guarantee we can always offset
	 * into the buffer by a few bytes.
	 */
	if (blen < g.c_data_max + 10) {
		if (buf != NULL) {
			free(buf);
			buf = NULL;
		}
		blen = g.c_data_max + 10;
		if ((buf = malloc(blen)) == NULL) {
			fprintf(stderr,
			    "%s: %s\n", g.progname, strerror(errno));
			exit(EXIT_FAILURE);
		}
		for (i = 0; i < blen; ++i)
			buf[i] = 'A' + i % 26;
	}

	/*
	 * The data always starts with a 10-digit string (the specified cnt), to
	 * ensure every data item is greater than the last data item -- if we're
	 * bulk-loading a duplicate data item, it must be larger than previous
	 * data items.
	 */
	sprintf(buf, "%010u", ++r);
	buf[10] = '/';

	switch (g.c_database_type) {
	case FIX:
		p = buf;
		if (g.c_repeat_comp != 0 ||
		    (u_int)wts_rand() % 100 <= g.c_repeat_comp_pct)
			p += wts_rand() % 7;
		len = g.c_data_min;
		break;
	case VAR:
	case ROW:
		p = buf;
		len = MMRAND(g.c_data_min, g.c_data_max);
		break;
	}

	data->data = p;
	data->size = len;
}

void
track(const char *s, u_int64_t i)
{
	static int lastlen = 0;
	int len;
	char *p, msg[128];

	if (!isatty(STDOUT_FILENO))
		return;

	if (s == NULL)
		len = 0;
	else if (i == 0)
		len = snprintf(msg, sizeof(msg), "%4d: %s", g.run_cnt, s);
	else
		len =
		    snprintf(msg, sizeof(msg), "%4d: %s %llu", g.run_cnt, s, i);

	for (p = msg + len; len < lastlen; ++len)
		*p++ = ' ';
	lastlen = len;
	for (; len > 0; --len)
		*p++ = '\b';
	*p = '\0';
	(void)printf("%s", msg);
	(void)fflush(stdout);
}

/*
 * wts_rand --
 *	Return a random number.
 */
int
wts_rand(void)
{
	char *p, buf[64];
	u_int r;

	/*
	 * We can entirely reproduce a run based on the random numbers used
	 * in the initial run, plus the configuration files.  It would be
	 * nice to just log the initial RNG seed, rather than logging every
	 * random number generated, but we can't -- Berkeley DB calls rand()
	 * internally, and so that messes up the pattern of random numbers
	 * (and WT might call rand() in the future, who knows?)
	 */
	if (g.rand_log == NULL) {
		p = fname(NULL, "rand");
		if ((g.rand_log = fopen(p, g.replay ? "r" : "w")) == NULL) {
			fprintf(stderr, p, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	if (g.replay) {
		if (fgets(buf, sizeof(buf), g.rand_log) == NULL) {
			if (feof(g.rand_log)) {
				fprintf(stderr,
				    "end of random number log reached, "
				    "exiting\n");
			} else
				fprintf(stderr,
				    "random number log: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		r = (u_int)strtoul(buf, NULL, 10);
	} else {
		r = (u_int)rand();
		fprintf(g.rand_log, "%u\n", r);
	}
	return ((int)r);
}
