/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Utility introduction: get the program name, and check the version. */
#define	WT_UTILITY_INTRO(prog, argv) do {				\
	int __major, __minor;						\
	if (((prog) = strrchr((argv)[0], '/')) == NULL)			\
		(prog) = (argv)[0];					\
	else								\
		++(prog);						\
	(void)wiredtiger_version(&__major, &__minor, NULL);		\
	if (__major != WIREDTIGER_VERSION_MAJOR ||			\
	    __minor != WIREDTIGER_VERSION_MINOR) {			\
		fprintf(stderr,						\
		    "%s: program build version %d.%d doesn't match "	\
		    "library build version %d.%d\n",			\
		    (prog), WIREDTIGER_VERSION_MAJOR,			\
		    WIREDTIGER_VERSION_MINOR, __major, __minor);	\
		return (EXIT_FAILURE);					\
	}								\
} while (0)
