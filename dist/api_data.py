# This file is a python script that describes the WiredTiger API.
# This data is used by the scripts api.py, api_err.py, config.py
#
#	2: a string of comma-separated configuration key words
#		extfunc  -- call an external function to do the work
#		getter	 -- getter method
#		connlock -- locks the connection mutex (implied by getter/setter)
#		method	 -- method returns an int
#		methodV  -- method returns void
#		noauto	 -- don't auto-generate a stub at all
#		rdonly	 -- not allowed if the database is read-only
#		restart	 -- handle WT_RESTART in the API call
#		rowonly	 -- row databases only
#		session	 -- function takes a SESSION/BTREE argument pair
#		setter	 -- setter method
#		verify	 -- setter methods call validation function

errors = [
	('WT_DEADLOCK', 'conflict with concurrent operation'),
	('WT_ERROR', 'non-specific WiredTiger error'),
	('WT_NOTFOUND', 'item not found'),
	('WT_READONLY', 'attempt to modify a read-only value'),
	('WT_RESTART', 'restart the operation (internal)'),
	('WT_TOOSMALL', 'buffer too small'),
]

class Method:
	def __init__(self, config, **flags):
		self.config = config
		self.flags = flags

class Config:
	def __init__(self, name, default, desc):
		self.name = name
		self.default = default
		self.desc = desc

methods = {
'cursor.close' : Method([]),

'session.close' : Method([]),

'session.create' : Method([
	Config('allocation_size', '512B', r'''
		file unit allocation size, in bytes'''),
	Config('columns', '', r'''
		list of the column names.  Comma-separated list of the form
		<code>(column[,...])</code>.  The number of entries must match the
		total number of values in \c key_format and \c value_format'''),
	Config('colgroup.name', '', r'''
		named group of columns to store together.  Comma-separated list of
		the form <code>(column[,...])</code>.  Each column group is stored
		separately, keyed by the primary key of the table.  Any column that
		does not appear in a column group is stored in a default, unnamed,
		column group for the table'''),
	Config('exclusive', 'false', r'''
		fail if the table exists (if "no", the default, verifies that the
		table exists and has the specified schema'''),
	Config('huffman_key', '', r'''
		use Huffman encoding for the key.  Permitted values are empty (off),
		\c "english" or \c "file:<filename>".  See @ref huffman for more
		details.'''),
	Config('huffman_value', '', r'''
		use Huffman encoding for the value.  Permitted values are empty (off),
		\c "english" or \c "file:<filename>".  See @ref huffman for more
		details.'''),
	Config('index.name', '', r'''
		named index on a list of columns.  Comma-separated list of the form
		<code>(column[,...])</code>'''),
	Config('intl_node_max', '2KB', r'''
		maximum page size for internal nodes, in bytes'''),
	Config('intl_node_min', '2KB', r'''
		minimum page size for internal nodes, in bytes'''),
	Config('key_format', 'u', r'''
		the format of the data packed into key items.  See @ref packing for
		details.  If not set, a default value of \c "u" is assumed, and
		applications use the WT_ITEM struct to manipulate raw byte arrays'''),
	Config('leaf_node_max', '1MB', r'''
		maximum page size for leaf nodes, in bytes'''),
	Config('leaf_node_min', '32KB', r'''
		minimum page size for leaf nodes, in bytes'''),
	Config('runlength_encoding', 'false', r'''
		compress repeated adjacent values'''),
	Config('value_format', 'u', r'''
		the format of the data packed into value items.  See @ref packing
		for details.  If not set, a default value of \c "u" is assumed, and
		applications use the WT_ITEM struct to manipulate raw byte arrays'''),
]),

'session.drop' : Method([
	Config('force', 'false', r'''
		return success if the object does not exist'''),
	]),

'session.log_printf' : Method([]),

'session.open_cursor' : Method([
	Config('bulk', 'false', r'''
		configure the cursor for bulk loads'''),
	Config('dump', 'false', r'''
		configure the cursor for dump format inputs and outputs'''),
	Config('isolation', 'read-committed', r'''
		the isolation level for this cursor, one of \c "snapshot" or
		\c "read-committed" or \c "read-uncommitted".
		Ignored for transactional cursors'''),
	Config('overwrite', 'false', r'''
		if an existing key is inserted, overwrite the existing value'''),
	Config('printable', 'false', r'''
		for dump cursors, pass through printable bytes unmodified'''),
	Config('raw', 'false', r'''
		ignore the encodings for the key and value, manage data as if the
		formats were \c "u"'''),
]),

'session.rename' : Method([]),
'session.salvage' : Method([]),
'session.sync' : Method([]),
'session.truncate' : Method([]),
'session.verify' : Method([]),

'session.begin_transaction' : Method([
	Config('isolation', 'read-committed', r'''
		the isolation level for this transaction, one of "serializable",
		"snapshot", "read-committed" or "read-uncommitted"; default
		"serializable"'''),
	Config('name', '', r'''
		name of the transaction for tracing and debugging'''),
	Config('sync', 'full', r'''
		how to sync log records when the transaction commits, one of
		"full", "flush", "write" or "none"'''),
	Config('priority', 0, r'''
		priority of the transaction for resolving conflicts, an integer
		between -100 and 100.  Transactions with higher values are less likely
		to abort'''),
]),

'session.commit_transaction' : Method([]),
'session.rollback_transaction' : Method([]),

'session.checkpoint' : Method([
	Config('archive', 'false', r'''
		remove log files no longer required for transactional durabilty'''),
	Config('force', 'false', r'''
		write a new checkpoint even if nothing has changed since the last
		one'''),
	Config('flush_cache', 'true', r'''
		flush the cache'''),
	Config('flush_log', 'true', r'''
		flush the log'''),
	Config('log_size', '0', r'''
		only proceed if more than the specified number of bytes of log
		records have been written since the last checkpoint'''),
	Config('timeout', '0', r'''
		only proceed if more than the specified number of milliseconds have
		elapsed since the last checkpoint'''),
]),

'connection.add_cursor_type' : Method([]),
'connection.add_collator' : Method([]),
'connection.add_extractor' : Method([]),
'connection.close' : Method([]),

'connection.load_extension' : Method([
	Config('entry', 'wiredtiger_extension_init', r'''
		the entry point of the extension'''),
	Config('prefix', '', r'''
		a prefix for all names registered by this extension (e.g., to make
		namespaces distinct or during upgrades'''),
]),

'connection.open_session' : Method([]),

'wiredtiger_open' : Method([
	Config('cache_size', '20MB', r'''
		maximum heap memory to allocate for the cache'''),
	Config('create', 'false', r'''
		create the database if it does not exist'''),
	Config('data_update_max', '32KB', r'''
		maximum update buffer size for a session'''),
	Config('data_update_min', '8KB', r'''
		minimum update buffer size for a session'''),
	Config('exclusive', 'false', r'''
		fail if the database already exists'''),
	Config('error_prefix', '', r'''
		Prefix string for error messages'''),
	Config('hazard_max', '15', r'''
		number of hazard references per session'''),
	Config('logging', 'false', r'''
		enable logging'''),
	Config('session_max', '50', r'''
		maximum expected number of sessions (including server threads)'''),
	Config('multiprocess', 'false', r'''
		permit sharing between processes (will automatically start an RPC
		server for primary processes and use RPC for secondary processes)'''),
	Config('verbose', '', r'''
		enable messages for various events.  One or more of "all", "evict",
		"fileops", "hazard", "mutex", "read".  Multiple options are given as
		a list such as \c "verbose=[evict,read]"'''),
]),
}

flags = {
###################################################
# Internal routine flag declarations
###################################################
	'bt_dump' : [ 'DEBUG', 'DUMP_PRINT' ],
	'bt_search_col' : [ 'WRITE' ],
	'bt_search_key_row' : [ 'WRITE' ],
	'bt_tree_walk' : [ 'WALK_CACHE' ],
	'huffman_set' : [ 'ASCII_ENGLISH', 'HUFFMAN_KEY', 'HUFFMAN_VALUE' ],
	'page_reconcile' : [ 'REC_EVICT', 'REC_LOCKED' ],
	'verbose' : [ 'VERB_EVICT', 'VERB_FILEOPS', 'VERB_HAZARD', 'VERB_MUTEX', 'VERB_READ' ],

###################################################
# Structure flag declarations
###################################################
	'conn' : [ 'SERVER_RUN', 'WORKQ_RUN' ],
	'buf' : [ 'BUF_INUSE' ],
	'session' : [ 'SESSION_INTERNAL' ],
}
