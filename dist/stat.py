# See the file LICENSE for redistribution information.
#
# Copyright (c) 2008 WiredTiger Software.
#	All rights reserved.
#
# $Id$

# Read the source files and output the statistics #defines and allocation code.

import re, string, sys
from dist import compare_srcfile
from dist import source_paths_list

# Read the source files and build a dictionary of handles and stat counters.
import stat_class

# print_def --
#	Print the #defines for the stat.h file.
def print_def(title, handle, list):
	def_cnt = 0
	f.write('/*\n')
	f.write(' * Statistics entries for ' + title + '.\n')
	f.write(' */\n')
	n = 'WT_STAT_' + handle  + '_TOTAL'
	f.write('#define\t' + n +
	    "\t" * max(1, 4 - len(n) / 8) + "%5d" % len(list) + '\n\n')
	for l in sorted(list.iteritems()):
		n = 'WT_STAT_' + l[0]
		f.write('#define\t' + n +
		    "\t" * max(1, 4 - len(n) / 8) + "%5d" % def_cnt + '\n')
		def_cnt += 1
	f.write('\n')

# Update the #defines in the stat.h file.
tmp_file = '__tmp'
f = open(tmp_file, 'w')
skip = 0
for line in open('../inc_posix/stat.h', 'r'):
	if not skip:
		f.write(line)
	if line.count('Statistics section: END'):
		f.write(line)
		skip = 0
	elif line.count('Statistics section: BEGIN'):
		f.write('\n')
		skip = 1
		print_def('ENV/IENV handle', 'ENV', stat_class.ienv_stats)
		print_def('DB/IDB handle', 'DB', stat_class.idb_stats)
		print_def('DB/IDB database', 'DATABASE', stat_class.idb_dstats)
		print_def('FH handle', 'FH', stat_class.fh_stats)
f.close()
compare_srcfile(tmp_file, '../inc_posix/stat.h')

# print_func --
#	Print the functions for the stat.c file.
def print_func(handle, list):
	f.write('\n')
	f.write('int\n')
	f.write('__wt_stat_alloc_' +
	    handle.lower() + '_stats(ENV *env, WT_STATS **statsp)\n')
	f.write('{\n')
	f.write('\tWT_STATS *stats;\n\n')
	f.write('\tWT_RET(__wt_calloc(env,\n')
	f.write('\t    WT_STAT_' +
	    handle + '_TOTAL + 1, sizeof(WT_STATS), &stats));\n\n')
	for l in sorted(list.iteritems()):
		o = '\tstats[WT_STAT_' + l[0] + '].desc = "' + l[1].str + '";\n'
		if len(o) + 7  > 80:
			o = o.replace('= ', '=\n\t    ')
		f.write(o)
	f.write('\n')
	f.write('\t*statsp = stats;\n')
	f.write('\treturn (0);\n')
	f.write('}\n')
	f.write('\n')

	f.write('void\n')
	f.write('__wt_stat_clear_' +
	    handle.lower() + '_stats(WT_STATS *stats)\n')
	f.write('{\n')
	for l in sorted(list.iteritems()):
		# Items marked permanent aren't cleared by the stat clear
		# methods.
		if not l[1].config.count('perm'):
			f.write('\tstats[WT_STAT_' + l[0] + '].v = 0;\n');
	f.write('}\n')

# Write the stat allocation, clear and print routines to the stat.c file.
f = open(tmp_file, 'w')
f.write('/* DO NOT EDIT: automatically built by dist/stat.py. */\n\n')
f.write('#include "wt_internal.h"\n')
print_func('ENV', stat_class.ienv_stats)
print_func('DB', stat_class.idb_stats)
print_func('DATABASE', stat_class.idb_dstats)
print_func('FH', stat_class.fh_stats)
f.close()
compare_srcfile(tmp_file, '../support/stat.c')
