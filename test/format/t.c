/*-
 * Copyright (c) 2008-2012 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "format.h"

GLOBAL g;

static void onint(int);
static void startup(void);
static void usage(void);

int
main(int argc, char *argv[])
{
	int ch, reps, ret;

	ret = 0;

	if ((g.progname = strrchr(argv[0], '/')) == NULL)
		g.progname = argv[0];
	else
		++g.progname;

	/* Configure the FreeBSD malloc for debugging. */
	(void)setenv("MALLOC_OPTIONS", "AJZ", 1);

	/* Set values from the "CONFIG" file, if it exists. */
	if (access("CONFIG", R_OK) == 0) {
		printf("... reading CONFIG file\n");
		config_file("CONFIG");
	}

	/* Default to a single thread. */
	g.threads = 1;

	/* Track progress unless we're re-directing output to a file. */
	g.track = isatty(STDOUT_FILENO) ? 1 : 0;

	/* Set values from the command line. */
	while ((ch = getopt(argc, argv, "1C:c:Llqrt:")) != EOF)
		switch (ch) {
		case '1':			/* One run */
			g.c_runs = 1;
			break;
		case 'C':			/* wiredtiger_open config */
			g.config_open = optarg;
			break;
		case 'c':			/* Configuration from a file */
			config_file(optarg);
			break;
		case 'L':			/* Re-direct output to a log */
			/*
			 * The -l option is a superset of -L, ignore -L if we
			 * have already configured logging for operations.
			 */
			if (g.logging == 0)
				g.logging = LOG_FILE;
			break;
		case 'l':			/* Turn on operation logging */
			g.logging = LOG_OPS;
			break;
		case 'q':			/* Quiet */
			g.track = 0;
			break;
		case 'r':			/* Replay a run */
			g.replay = 1;
			g.c_runs = 1;
			break;
		case 't':			/* Threads */
			g.threads = atoi(optarg);
			break;
		default:
			usage();
		}

	/* Multi-threaded runs cannot be replayed. */
	if (g.threads != 1 && g.replay) {
		fprintf(stderr, "%s: -r and -t are mutually exclusive\n"); 
		return (EXIT_FAILURE);
	}

	argc -= optind;
	argv += optind;
	for (; *argv != NULL; ++argv)
		config_single(*argv, 1);

	/* Clean up on signal. */
	(void)signal(SIGINT, onint);

	printf("%s: process %" PRIdMAX "\n", g.progname, (intmax_t)getpid());
	while (++g.run_cnt <= g.c_runs || g.c_runs == 0 ) {
		startup();			/* Start a run */

		config_setup();			/* Run configuration */
		config_print(0);		/* Dump run configuration */
		key_len_setup();		/* Setup keys */

		if (SINGLETHREADED)
			bdb_startup();		/* Initial file config */
		if (wts_startup())
			return (EXIT_FAILURE);

		if (wts_bulk_load())		/* Load initial records */
			goto err;
						/* Close, verify */
		if (wts_teardown() || wts_verify("post-bulk verify"))
			goto err;
						/* Loop reading & operations */
		for (reps = 0; reps < 3; ++reps) {
			if (wts_startup())
				goto err;

			if (wts_read_scan())	/* Read scan */
				goto err;

						/* Random operations */
			if (g.c_ops != 0 && wts_ops())
				goto err;

						/* Statistics */
			if ((g.c_ops == 0 || reps == 2) && wts_stats())
				goto err;

						/* Close, verify */
			if (wts_teardown() || wts_verify("post-ops verify"))
				goto err;

			/*
			 * If no operations scheduled, quit after a single
			 * read pass.
			 */
			if (g.c_ops == 0)
				break;
		}

		if (SINGLETHREADED) {
			track("shutting down BDB", 0ULL);
			bdb_teardown();

			if (wts_dump("standard", 1))	/* Dump the file */
				goto err;
		}

		/*
		 * If we don't delete any records, we can salvage the file.  The
		 * problem with deleting records is that salvage will restore
		 * deleted records if a page fragments leaving a deleted record
		 * on one side of the split.
		 *
		 * Save a copy, salvage, verify, dump.
		 */
		if (g.c_delete_pct == 0) {
			/*
			 * Save a copy of the interesting files so we can replay
			 * the salvage step as necessary.
			 */
			if (system(
			    "rm -rf __slvg.copy && "
			    "mkdir __slvg.copy && "
			    "cp WiredTiger* __wt __slvg.copy/") != 0)
				goto err;

			if (wts_salvage() ||
			    wts_verify("post-salvage verify") ||
			    wts_dump("salvage", 0))
				goto err;
		}

		printf("%4d: %-40s\n", g.run_cnt, config_dtype());
	}

	if (0) {
err:		ret = 1;
	}

	/* Flush/close any logging information. */
	if (g.logfp != NULL)
		(void)fclose(g.logfp);
	if (g.rand_log != NULL)
		(void)fclose(g.rand_log);

	config_print(ret);
	return (ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * startup --
 *	Initialize for a run.
 */
static void
startup(void)
{
	/* Close the logging file. */
	if (g.logfp != NULL) {
		(void)fclose(g.logfp);
		g.logfp = NULL;
	}

	/* Close the random number file. */
	if (g.rand_log != NULL) {
		(void)fclose(g.rand_log);
		g.rand_log = NULL;
	}

	/* Remove the run's files except for __rand. */
	(void)system("rm -rf WiredTiger WiredTiger.* __[a-qs-z]* __run");

	/* Open/truncate the logging file. */
	if (g.logging != 0) {
		if ((g.logfp = fopen("__log", "w")) == NULL)
			die("__log", errno);
		(void)setvbuf(g.logfp, NULL, _IOLBF, 0);
	}
}

/*
 * onint --
 *	Interrupt signal handler.
 */
static void
onint(int signo)
{
	UNUSED(signo);

	/* Remove the run's files except for __rand. */
	(void)system("rm -rf WiredTiger WiredTiger.* __[a-qs-z]* __run");

	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

/*
 * die --
 *	Report an error and quit.
 */
void
die(const char *m, int e)
{
	/* Flush/close any logging information. */
	if (g.logfp != NULL)
		(void)fclose(g.logfp);
	if (g.rand_log != NULL)
		(void)fclose(g.rand_log);

	config_print(1);

	fprintf(stderr, "%s: %s: %s\n", g.progname, m, wiredtiger_strerror(e));
	exit(EXIT_FAILURE);
}

/*
 * usage --
 *	Display usage statement and exit failure.
 */
static void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-1Llqr]\n    "
	    "[-C wiredtiger-config] [-c config-file] "
	    "[-t threads] [name=value ...]\n",
	    g.progname);
	fprintf(stderr, "%s",
	    "\t-1 run once\n"
	    "\t-C specify wiredtiger_open configuration arguments\n"
	    "\t-c read test program configuration from a file\n"
	    "\t-L output to a log file\n"
	    "\t-l log operations (implies -L)\n"
	    "\t-q run quietly\n"
	    "\t-r replay the last run\n"
	    "\t-t threads\n");

	fprintf(stderr, "\n");

	config_error();
	exit(EXIT_FAILURE);
}
