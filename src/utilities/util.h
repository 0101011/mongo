/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wt_internal.h>

#define	UTIL_FILE_OK	0x01			/* file: prefix OK */
#define	UTIL_TABLE_OK	0x02			/* table: prefix OK */

typedef struct {
	void   *mem;				/* Managed memory chunk */
	size_t	memsize;			/* Managed memory size */
} ULINE;

extern const char *progname;			/* Program name */
extern const char *usage_prefix;		/* Global arguments */
extern int verbose;				/* Verbose flag */

extern WT_EVENT_HANDLER *verbose_handler;

/*
 * We compile in own version of getopt, it's simpler than figuring out what the
 * system has.
 */
extern int   util_opterr;		/* if error message should be printed */
extern int   util_optind;		/* index into parent argv vector */
extern int   util_optopt;		/* character checked for validity */
extern int   util_optreset;		/* reset getopt */
extern char *util_optarg;		/* argument associated with option */

int	 util_cerr(const char *, const char *, int);
void	 util_copyright(void);
int	 util_create(WT_SESSION *, int, char *[]);
int	 util_drop(WT_SESSION *, int, char *[]);
int	 util_dump(WT_SESSION *, int, char *[]);
int	 util_dumpfile(WT_SESSION *, int, char *[]);
int	 util_err(int, const char *, ...);
int	 util_getopt(int, char * const *, const char *);
int	 util_list(WT_SESSION *, int, char *[]);
int	 util_load(WT_SESSION *, int, char *[]);
int	 util_loadtext(WT_SESSION *, int, char *[]);
char	*util_name(const char *, const char *, u_int);
int	 util_printlog(WT_SESSION *, int, char *[]);
int	 util_read(WT_SESSION *, int, char *[]);
int	 util_read_line(ULINE *, int, int *);
int	 util_rename(WT_SESSION *, int, char *[]);
int	 util_salvage(WT_SESSION *, int, char *[]);
int	 util_stat(WT_SESSION *, int, char *[]);
int	 util_str2recno(const char *p, uint64_t *recnop);
int	 util_upgrade(WT_SESSION *, int, char *[]);
int	 util_verify(WT_SESSION *, int, char *[]);
int	 util_write(WT_SESSION *, int, char *[]);
