/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef BDB
#include "build_unix/db.h"
#else
#include "wt_internal.h"
#endif

/* General purpose. */
#define	M(v)	((v) * 1000000)			/* Million */

/* Get a random value between a min/max pair. */
#define	MMRAND(min, max)	(wts_rand() % ((max + 1) - (min)) + (min))

#define	FIX		1			/* File types */
#define	ROW		2
#define	VAR		3

#define	BDB_PREFIX	"bdb"
#define	WT_PREFIX	"wt"
#define	WT_TABLENAME	"file:__wt"

typedef struct {
	char *progname;				/* Program name */

	void *bdb;				/* BDB comparison handle */

	void *wts_conn;				/* WT_CONNECTION handle */
	void *wts_cursor;			/* WT_CURSOR handle */
	void *wts_session;			/* WT_SESSION handle */

	FILE *rand_log;				/* Random number log */

	uint32_t run_cnt;			/* Run counter */

	int logging;				/* Are we logging everything? */
	FILE *logfp;				/* Log file. */
	int replay;				/* Replaying a run. */
	int track;				/* Track progress */
	int verbose;				/* Verbosity */

	char *key_gen_buf;

	uint32_t c_cache;			/* Config values */
	uint32_t c_data_max;
	uint32_t c_data_min;
	uint32_t c_data_fix;
	uint32_t c_delete_pct;
	uint32_t c_file_type;
	uint32_t c_huffman_key;
	uint32_t c_huffman_value;
	uint32_t c_insert_pct;
	uint32_t c_intl_node_max;
	uint32_t c_intl_node_min;
	uint32_t c_key_max;
	uint32_t c_key_min;
	uint32_t c_leaf_node_max;
	uint32_t c_leaf_node_min;
	uint32_t c_ops;
	uint32_t c_repeat_data_pct;
	uint32_t c_rows;
	uint32_t c_runs;
	uint32_t c_write_pct;

	uint32_t key_cnt;			/* Keys loaded so far */
	uint16_t key_rand_len[1031];		/* Key lengths */
} GLOBAL;
extern GLOBAL g;

int	 bdb_del(uint64_t, int *);
void	 bdb_insert(const void *, uint32_t, const void *, uint32_t);
int	 bdb_put(const void *, uint32_t, const void *, uint32_t, int *);
int	 bdb_read(uint64_t, void *, uint32_t *, int *);
void	 bdb_startup(void);
void	 bdb_teardown(void);
const char *
	 config_dtype(void);
void	 config_file(const char *);
void	 config_names(void);
void	 config_print(int);
void	 config_setup(void);
void	 config_single(char *, int);
void	 value_gen(void *, uint32_t *);
char	*fname(const char *);
void	 key_gen(void *, uint32_t *, uint64_t, int);
void	 key_gen_setup(void);
void	 track(const char *, uint64_t);
int	 wts_bulk_load(void);
int	 wts_dump(const char *, int);
int	 wts_ops(void);
uint32_t wts_rand(void);
int	 wts_read_scan(void);
int	 wts_salvage(void);
int	 wts_startup(void);
int	 wts_stats(void);
int	 wts_teardown(void);
int	 wts_verify(const char *);
