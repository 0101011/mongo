/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

/*
 * WT_PAGE_MODIFY --
 *	When a page is modified, there's additional information maintained as it
 * is written to disk.
 */
typedef enum {
	WT_PT_EMPTY=0,			/* Unused slot */
	WT_PT_BLOCK,			/* Block: inactive */
	WT_PT_BLOCK_EVICT,		/* Block: inactive on eviction */
	WT_PT_OVFL,			/* Overflow: active */
	WT_PT_OVFL_DISCARD		/* Overflow: inactive */
} __wt_pt_type_t;

struct __wt_page_modify {
	/*
	 * The write generation is incremented after a page is modified.  That
	 * is, it tracks page versions.
	 *
	 * The write generation value is used to detect changes scheduled based
	 * on out-of-date information.  Two threads of control updating the same
	 * page could both search the page in state A.  When the updates are
	 * performed serially, one of the changes will happen after the page is
	 * modified, and the search state for the other thread might no longer
	 * be applicable.  To avoid this race, page write generations are copied
	 * into the search stack whenever a page is read, and check when a
	 * modification is serialized.  The serialized function compares each
	 * page's current write generation to the generation copied in the
	 * read/search; if the two values match, the search occurred on a
	 * current version of the page and the modification can proceed.  If the
	 * two generations differ, the serialized call returns an error and the
	 * operation must be restarted.
	 *
	 * The write-generation value could be stored on a per-entry basis if
	 * there's sufficient contention for the page as a whole.
	 *
	 * The write-generation is not declared volatile: write-generation is
	 * written by a serialized function when modifying a page, and must be
	 * flushed in order as the serialized updates are flushed.
	 *
	 * XXX
	 * 32-bit values are probably more than is needed: at some point we may
	 * need to clean up pages once there have been sufficient modifications
	 * to make our linked lists of inserted cells too slow to search, or as
	 * soon as enough memory is allocated in service of page modifications
	 * (although we should be able to release memory from the MVCC list as
	 * soon as there's no running thread/txn which might want that version
	 * of the data).   I've used 32-bit types instead of 16-bit types as I
	 * am less confident a 16-bit write to memory will be atomic.
	 */
	uint32_t write_gen;

	/*
	 * The disk generation tracks page versions written to disk.  When a
	 * page is reconciled and written to disk, the thread doing that work
	 * is just another reader of the page, and other readers and writers
	 * can access the page at the same time.  For this reason, the thread
	 * reconciling the page logs the write generation of the page it read.
	 */
	uint32_t disk_gen;

	union {
		WT_PAGE *write_split;	/* Newly created internal pages */
		WT_OFF	 write_off;	/* Newly written page */
	} u;

	/*
	 * Track pages, blocks to discard: as pages are reconciled, overflow
	 * K/V items are discarded along with their underlying blocks, and as
	 * pages are evicted, split and emptied pages are merged into their
	 * parents and discarded.  If an overflow item was discarded and page
	 * reconciliation then failed, the in-memory tree would be corrupted.
	 * To keep the tree correct until we're sure page reconciliation has
	 * succeeded, we track the objects we'll discard when the reconciled
	 * page is evicted.
	 *
	 * Track overflow objects: if pages are reconciled more than once, an
	 * overflow item might be written repeatedly.  Instead, when overflow
	 * items are written we save a copy and resulting location so we only
	 * write them once.
	 */
	struct __wt_page_track {
		__wt_pt_type_t type;	/* Type */

		uint8_t *data;		/* Overflow data reference */
		uint32_t len;		/* Overflow data length */
		uint32_t addr;		/* Block location */
		uint32_t size;		/* Block length */
	} *track;			/* Array of tracked objects */
	uint32_t track_entries;		/* Total track slots */
};

/*
 * WT_PAGE --
 * The WT_PAGE structure describes the in-memory information about a disk page.
 */
struct __wt_page {
	/*
	 * Two links to the parent's WT_PAGE structure -- the physical parent
	 * page, and the WT_REF structure used to find this page.
	 */
#define	WT_PAGE_IS_ROOT(page)						\
	((page)->parent == NULL)
	WT_PAGE	*parent;		/* Page's parent */
	WT_REF	*parent_ref;		/* Page's parent reference */

	/* But the entries are wildly different, based on the page type. */
	union {
		/* Row-store internal page. */
		struct {
			WT_ROW_REF *t;		/* Subtrees */
		} row_int;

		/* Row-store leaf page. */
		struct {
			WT_ROW	   *d;		/* K/V object pairs */
			WT_INSERT_HEAD **ins;	/* Inserts */
			WT_UPDATE **upd;	/* Updates */
		} row_leaf;

		/* Column-store internal page. */
		struct {
			uint64_t    recno;	/* Starting recno */
			WT_COL_REF *t;		/* Subtrees */
		} col_int;

		/* Column-store leaf page. */
		struct {
			uint64_t    recno;	/* Starting recno */

			uint8_t	   *bitf;	/* COL_FIX items */
			WT_COL	   *d;		/* COL_VAR items */

			/*
			 * The last page of both fix- and variable-length column
			 * stores includes a skiplist of appended entries.
			 */
			WT_INSERT_HEAD **append;/* Appended items */

			/*
			 * Updated items in column-stores: variable-length RLE
			 * entries can expand to multiple entries which requires
			 * some kind of list we can expand on demand.  Updated
			 * items in fixed-length files could be done based on an
			 * WT_UPDATE array as in row-stores, but there can be a
			 * very large number of bits on a single page, and the
			 * cost of the WT_UPDATE array would be huge.
			 */
			WT_INSERT_HEAD **update;/* Updated items */

			/*
			 * Variable-length column-store files maintain a list of
			 * RLE entries on the page so it's unnecessary to walk
			 * the page counting records to find a specific entry.
			 */
			WT_COL_RLE *repeats;	/* RLE array for lookups */
			uint32_t    nrepeats;	/* Number of repeat slots. */
		} col_leaf;
	} u;

	/* Page's on-disk representation: NULL for pages created in memory. */
	WT_PAGE_DISK *dsk;

	/* If/when the page is modified, we need lots more information. */
	WT_PAGE_MODIFY *modify;

	/*
	 * The read generation is incremented each time the page is searched,
	 * and acts as an LRU value for each page in the tree; it is read by
	 * the eviction server thread to select pages to be discarded from the
	 * in-memory tree.
	 *
	 * The read generation is a 64-bit value; incremented every time the
	 * page is searched, a 32-bit value could overflow.
	 *
	 * The read-generation is not declared volatile: read-generation is set
	 * a lot (on every access), and we don't want to write it that much.
	 */
	 uint64_t read_gen;

	/*
	 * In-memory pages optionally reference a number of entries originally
	 * read from disk.
	 */
	uint32_t entries;

	/*
	 * In-memory pages may have an optional memory allocation; this field
	 * is only so the appropriate calculations are done when the page is
	 * discarded.
	 */
	uint32_t memory_footprint;

#define	WT_PAGE_INVALID		0	/* Invalid page */
#define	WT_PAGE_COL_FIX		1	/* Col-store fixed-len leaf */
#define	WT_PAGE_COL_INT		2	/* Col-store internal page */
#define	WT_PAGE_COL_VAR		3	/* Col-store var-length leaf page */
#define	WT_PAGE_OVFL		4	/* Overflow page */
#define	WT_PAGE_ROW_INT		5	/* Row-store internal page */
#define	WT_PAGE_ROW_LEAF	6	/* Row-store leaf page */
#define	WT_PAGE_FREELIST	7	/* Free-list page */
	uint8_t type;			/* Page type */

	/*
	 * The flags are divided into two sets: flags set initially, before more
	 * than a single thread accesses the page, and the reconciliation flags.
	 * The alternative would be to move the WT_PAGE_REC_XXX flags into the
	 * WT_PAGE_MODIFY structure, but that costs more memory.  Obviously, it
	 * is important not to add more flags that can be set at run-time, else
	 * the threads could race.
	 */
#define	WT_PAGE_BUILD_KEYS	0x001	/* Keys have been built in memory */
#define	WT_PAGE_FORCE_EVICT	0x002	/* Waiting for forced eviction */
#define	WT_PAGE_PINNED		0x004	/* Page is pinned */
#define	WT_PAGE_REC_EMPTY	0x008	/* Reconciliation: page empty */
#define	WT_PAGE_REC_REPLACE	0x010	/* Reconciliation: page replaced */
#define	WT_PAGE_REC_SPLIT	0x020	/* Reconciliation: page split */
#define	WT_PAGE_REC_SPLIT_MERGE	0x040	/* Reconciliation: page split merge */
	uint8_t flags;			/* Page flags */
};

#define	WT_PAGE_REC_MASK						\
	(WT_PAGE_REC_EMPTY |						\
	    WT_PAGE_REC_REPLACE | WT_PAGE_REC_SPLIT | WT_PAGE_REC_SPLIT_MERGE)

/*
 * WT_PADDR, WT_PSIZE --
 *	A page's address and size.  We don't maintain the page's address/size in
 * the page: a page's address/size is found in the page parent's WT_REF struct,
 * and like a person with two watches can never be sure what time it is, having
 * two places to find a piece of information leads to confusion.
 */
#define	WT_PADDR(p)	((p)->parent_ref->addr)
#define	WT_PSIZE(p)	((p)->parent_ref->size)

/*
 * WT_REF --
 * A single in-memory page and the state information used to determine if it's
 * OK to dereference the pointer to the page.
 *
 * Synchronization is based on the WT_REF->state field, which has 3 states:
 *
 * WT_REF_DISK:
 *      The default setting before any pages are brought into memory, and set
 *	by the eviction server after page reconciliation (when the page has
 *	been discarded or written to disk, and remains backed by the disk);
 *	the page is on disk, and needs to be read into memory before use.
 * WT_REF_LOCKED:
 *	Set by the eviction server; the eviction server has selected this page
 *	for eviction and is checking hazard references.
 * WT_REF_MEM:
 *	Set by the read server when the page is read from disk; the page is
 *	in the cache and the page reference is OK.
 *
 * The life cycle of a typical page goes like this: pages are read into memory
 * from disk and the read server sets their state to WT_REF_MEM.  When the
 * eviction server selects the page for eviction, it sets the page state to
 * WT_REF_LOCKED.  In all cases, the eviction server resets the page's state
 * when it's finished with the page: if eviction was successful (a clean page
 * was simply discarded, and a dirty page was written to disk), the server sets
 * the page state to WT_REF_DISK; if eviction failed because the page was busy,
 * the page state is reset to WT_REF_MEM.
 *
 * Readers check the state field and if it's WT_REF_MEM, they set a hazard
 * reference to the page, flush memory and re-confirm the page state.  If the
 * page state is unchanged, the reader has a valid reference and can proceed.
 *
 * When the eviction server wants to discard a page from the tree, it sets the
 * WT_REF_LOCKED flag, flushes memory, then checks hazard references.  If the
 * eviction server finds a hazard reference, it resets the state to WT_REF_MEM,
 * restoring the page to the readers.  If the eviction server does not find a
 * hazard reference, the page is evicted.
 */
struct __wt_ref {
	WT_PAGE *page;			/* In-memory page */

	/*
	 * Page state.
	 *
	 * WT_REF_DISK has a value of 0, the default state after allocating
	 * cleared memory.
	 */
#define	WT_REF_DISK		0	/* Page is on disk */
#define	WT_REF_LOCKED		1	/* Page being evaluated for eviction */
#define	WT_REF_MEM		2	/* Page is in cache and valid */
#define	WT_REF_READING		3	/* Page being read */
	uint32_t volatile state;

	uint32_t addr;			/* Backing disk address */
	uint32_t size;			/* Backing disk size */
};

/*
 * WT_IKEY --
 * Instantiated key: row-store keys are usually prefix compressed and sometimes
 * Huffman encoded or overflow objects.  Normally, a row-store page in-memory
 * key points to the on-page WT_CELL, but in some cases, we instantiate the key
 * in memory, in which case the row-store page in-memory key points to a WT_IKEY
 * structure.
 */
struct __wt_ikey {
	WT_SESSION_BUFFER *sb;		/* Session buffer holding the WT_IKEY */

	uint32_t size;			/* Key length */

	/*
	 * If we no longer point to the key's on-page WT_CELL, we can't find its
	 * related value.  Save the offset of the key cell in the page.
	 *
	 * Row-store cell references are page offsets, not pointers (we boldly
	 * re-invent short pointers).  The trade-off is 4B per K/V pair on a
	 * 64-bit machine vs. a single cycle for the addition of a base pointer.
	 */
	uint32_t  cell_offset;

	/* The key bytes immediately follow the WT_IKEY structure. */
#define	WT_IKEY_DATA(ikey)						\
	((void *)((uint8_t *)(ikey) + sizeof(WT_IKEY)))
};

/*
 * WT_ROW_REF --
 * Row-store internal page subtree entries.
 */
struct __wt_row_ref {
	WT_REF	 ref;			/* Subtree page */
#define	WT_ROW_REF_ADDR(rref)	((rref)->ref.addr)
#define	WT_ROW_REF_PAGE(rref)	((rref)->ref.page)
#define	WT_ROW_REF_SIZE(rref)	((rref)->ref.size)
#define	WT_ROW_REF_STATE(rref)	((rref)->ref.state)

	void	*key;			/* On-page cell or off-page WT_IKEY */
};

/*
 * WT_ROW_REF_FOREACH --
 * Macro to walk the off-page subtree array of an in-memory internal page.
 */
#define	WT_ROW_REF_FOREACH(page, rref, i)				\
	for ((i) = (page)->entries,					\
	    (rref) = (page)->u.row_int.t; (i) > 0; ++(rref), --(i))

/*
 * WT_ROW_REF_SLOT --
 *	Return the 0-based array offset based on a WT_ROW_REF reference.
 */
#define	WT_ROW_REF_SLOT(page, rref)					\
	((uint32_t)(((WT_ROW_REF *)rref) - (page)->u.row_int.t))

/*
 * WT_COL_REF --
 * Column-store internal page subtree entries.
 */
struct __wt_col_ref {
	WT_REF	 ref;			/* Subtree page */
#define	WT_COL_REF_ADDR(cref)	((cref)->ref.addr)
#define	WT_COL_REF_PAGE(cref)	((cref)->ref.page)
#define	WT_COL_REF_SIZE(cref)	((cref)->ref.size)
#define	WT_COL_REF_STATE(cref)	((cref)->ref.state)

	uint64_t recno;			/* Starting record number */
};

/*
 * WT_COL_REF_FOREACH --
 * Macro to walk the off-page subtree array of an in-memory internal page.
 */
#define	WT_COL_REF_FOREACH(page, cref, i)				\
	for ((i) = (page)->entries,					\
	    (cref) = (page)->u.col_int.t; (i) > 0; ++(cref), --(i))

/*
 * WT_COL_REF_SLOT --
 *	Return the 0-based array offset based on a WT_COL_REF reference.
 */
#define	WT_COL_REF_SLOT(page, cref)					\
	((uint32_t)(((WT_COL_REF *)cref) - (page)->u.col_int.t))

/*
 * WT_ROW --
 * Each in-memory page row-store leaf page has an array of WT_ROW structures:
 * this is created from on-page data when a page is read from the file.  It's
 * sorted by key, fixed in size, and references data on the page.
 */
struct __wt_row {
	void	*key;			/* On-page cell or off-page WT_IKEY */
};

/*
 * WT_ROW_FOREACH --
 *	Walk the entries of an in-memory row-store leaf page.
 */
#define	WT_ROW_FOREACH(page, rip, i)					\
	for ((i) = (page)->entries,					\
	    (rip) = (page)->u.row_leaf.d; (i) > 0; ++(rip), --(i))
#define	WT_ROW_FOREACH_REVERSE(page, rip, i)				\
	for ((i) = (page)->entries,					\
	    (rip) = (page)->u.row_leaf.d + ((page)->entries - 1);	\
	    (i) > 0; --(rip), --(i))

/*
 * WT_ROW_SLOT --
 *	Return the 0-based array offset based on a WT_ROW reference.
 */
#define	WT_ROW_SLOT(page, rip)						\
	((uint32_t)(((WT_ROW *)rip) - (page)->u.row_leaf.d))

/*
 * WT_COL --
 * Each in-memory variable-length column-store leaf page has an array of WT_COL
 * structures: this is created from on-page data when a page is read from the
 * file.  It's fixed in size, and references data on the page.
 */
struct __wt_col {
	/*
	 * Variable-length column-store data references are page offsets, not
	 * pointers (we boldly re-invent short pointers).  The trade-off is 4B
	 * per K/V pair on a 64-bit machine vs. a single cycle for the addition
	 * of a base pointer.  The on-page data is a WT_CELL (same as row-store
	 * pages).
	 *
	 * If the value is 0, it's a single, deleted record.   While this might
	 * be marginally faster than looking at the page, the real reason for
	 * this is to simplify extending column-store files: a newly allocated
	 * WT_COL array translates to a set of deleted records, which is exactly
	 * what we want.
	 *
	 * Obscure the field name, code shouldn't use WT_COL->value, the public
	 * interface is WT_COL_PTR.
	 */
	uint32_t __value;
};

/*
 * WT_COL_RLE --
 * In variable-length column store leaf pages, we build an array of entries
 * with RLE counts greater than 1 when reading the page.  We can do a binary
 * search in this array, then an offset calculation to find the cell.
 */
struct __wt_col_rle {
	uint64_t recno;			/* Record number of first repeat. */
	uint64_t rle;			/* Repeat count. */
	uint32_t indx;			/* Slot of entry in col_leaf.d */
} WT_GCC_ATTRIBUTE((packed));

/*
 * WT_COL_PTR --
 *     Return a pointer corresponding to the data offset -- if the item doesn't
 * exist on the page, return a NULL.
 */
#define	WT_COL_PTR(page, cip)						\
	((cip)->__value == 0 ? NULL : WT_REF_OFFSET(page, (cip)->__value))

/*
 * WT_COL_FOREACH --
 *	Walk the entries of an in-memory column-store leaf page.
 */
#define	WT_COL_FOREACH(page, cip, i)					\
	for ((i) = (page)->entries,					\
	    (cip) = (page)->u.col_leaf.d; (i) > 0; ++(cip), --(i))

/*
 * WT_COL_SLOT --
 *	Return the 0-based array offset based on a WT_COL reference.
 */
#define	WT_COL_SLOT(page, cip)						\
	((uint32_t)(((WT_COL *)cip) - (page)->u.col_leaf.d))

/*
 * WT_UPDATE --
 * Entries on leaf pages can be updated, either modified or deleted.  Updates
 * to entries referenced from the WT_ROW and WT_COL arrays are stored in the
 * page's WT_UPDATE array.  When the first element on a page is updated, the
 * WT_UPDATE array is allocated, with one slot for every existing element in
 * the page.  A slot points to a WT_UPDATE structure; if more than one update
 * is done for an entry, WT_UPDATE structures are formed into a forward-linked
 * list.
 */
struct __wt_update {
	WT_SESSION_BUFFER *sb;		/* session buffer holding this update */

	WT_UPDATE *next;		/* forward-linked list */

	/*
	 * We can't store 4GB cells: we're short by a few bytes because each
	 * change/insert item requires a leading WT_UPDATE structure.  For that
	 * reason, we can use the maximum size as an is-deleted flag and don't
	 * have to increase the size of this structure for a flag bit.
	 */
#define	WT_UPDATE_DELETED_ISSET(upd)	((upd)->size == UINT32_MAX)
#define	WT_UPDATE_DELETED_SET(upd)	((upd)->size = UINT32_MAX)
	uint32_t size;			/* update length */

	/* The untyped value immediately follows the WT_UPDATE structure. */
#define	WT_UPDATE_DATA(upd)						\
	((void *)((uint8_t *)(upd) + sizeof(WT_UPDATE)))
};

/*
 * WT_INSERT --
 *
 * Row-store leaf pages support inserts of new K/V pairs.  When the first K/V
 * pair is inserted, the WT_INSERT array is allocated, with one slot for every
 * existing element in the page, plus one additional slot.  A slot points to a
 * WT_INSERT_HEAD structure for the items which sort after the WT_ROW element
 * that references it and before the subsequent WT_ROW element; if more than
 * one insert is done between two page entries, the WT_INSERT structures are
 * formed into a key-sorted skip list.  The skip list structure has a randomly
 * chosen depth of next pointers in each inserted node.
 *
 * The additional slot is because it's possible to insert items smaller than
 * any existing key on the page -- for that reason, the first slot of the
 * insert array holds keys smaller than any other key on the page.
 *
 * In column-store variable-length run-length encoded pages, a single indx
 * entry may reference a large number of records, because there's a single
 * on-page entry representing many identical records.   (We don't expand those
 * entries when the page comes into memory, as that would require resources as
 * pages are moved to/from the cache, including read-only files.)  Instead, a
 * single indx entry represents all of the identical records originally found
 * on the page.
 *
 * Modifying (or deleting) run-length encoded column-store records is hard
 * because the page's entry no longer references a set of identical items.  We
 * handle this by "inserting" a new entry into the insert array, with its own
 * record number.  (This is the only case where it's possible to insert into a
 * column-store: only appends are allowed, as insert requires re-numbering
 * subsequent records.  Berkeley DB did support mutable records, but it won't
 * scale and it isn't useful enough to re-implement, IMNSHO.)
 */
struct __wt_insert {
	WT_SESSION_BUFFER *sb;			/* insert session buffer */

	WT_UPDATE *upd;				/* value */

	union {
		uint64_t recno;			/* column-store record number */
		struct {
			uint32_t offset;	/* row-store key data start */
			uint32_t size;          /* row-store key data size */
		} key;
	} u;

#define	WT_INSERT_KEY_SIZE(ins) ((ins)->u.key.size)
#define	WT_INSERT_KEY(ins)						\
	((void *)((uint8_t *)(ins) + (ins)->u.key.offset))
#define	WT_INSERT_RECNO(ins)	((ins)->u.recno)

	WT_INSERT *next[0];			/* forward-linked skip list */
};

/* 10 level skip lists, 1/2 have a link to the next element. */
#define	WT_SKIP_MAXDEPTH        10
#define	WT_SKIP_PROBABILITY     (UINT32_MAX >> 2)

/*
 * Skiplist helper macros.
 */
#define	WT_SKIP_FIRST(ins_head)						\
	(((ins_head) == NULL) ? NULL : (ins_head)->head[0])
#define	WT_SKIP_LAST(ins_head)						\
	(((ins_head) == NULL) ? NULL : (ins_head)->tail[0])
#define	WT_SKIP_NEXT(ins)  ((ins)->next[0])
#define	WT_SKIP_FOREACH(ins, ins_head)					\
	for ((ins) = WT_SKIP_FIRST(ins_head);				\
	    (ins) != NULL;						\
	    (ins) = WT_SKIP_NEXT(ins))

/*
 * WT_INSERT_HEAD --
 * 	The head of a skip list of WT_INSERT items.
 */
struct __wt_insert_head {
	WT_INSERT *head[WT_SKIP_MAXDEPTH];	/* first item on skiplists */
	WT_INSERT *tail[WT_SKIP_MAXDEPTH];	/* last item on skiplists */
};

/*
 * The row-store leaf page insert lists are arrays of pointers to structures,
 * and may not exist.  The following macros return an array entry if the array
 * of pointers and the specific structure exist, else NULL.
 */
#define	WT_ROW_INSERT_SLOT(page, slot)					\
	((page)->u.row_leaf.ins == NULL ?				\
	    NULL : (page)->u.row_leaf.ins[slot])
#define	WT_ROW_INSERT(page, ip)						\
	WT_ROW_INSERT_SLOT(page, WT_ROW_SLOT(page, ip))
#define	WT_ROW_UPDATE(page, ip)						\
	((page)->u.row_leaf.upd == NULL ?				\
	    NULL : (page)->u.row_leaf.upd[WT_ROW_SLOT(page, ip)])
/*
 * WT_ROW_INSERT_SMALLEST references an additional slot past the end of the
 * the "one per WT_ROW slot" insert array.  That's because the insert array
 * requires an extra slot to hold keys that sort before any key found on the
 * original page.
 */
#define	WT_ROW_INSERT_SMALLEST(page)					\
	((page)->u.row_leaf.ins == NULL ?				\
	    NULL : (page)->u.row_leaf.ins[(page)->entries])

/*
 * The column-store leaf page update lists are arrays of pointers to structures,
 * and may not exist.  The following macros return an array entry if the array
 * of pointers and the specific structure exist, else NULL.
 */
#define	WT_COL_UPDATE_SLOT(page, slot)					\
	((page)->u.col_leaf.update == NULL ?				\
	    NULL : (page)->u.col_leaf.update[slot])
#define	WT_COL_UPDATE(page, ip)						\
	WT_COL_UPDATE_SLOT(page, WT_COL_SLOT(page, ip))

/*
 * WT_COL_UPDATE_SINGLE is a single WT_INSERT list, used for any fixed-length
 * column-store updates for a page.
 */
#define	WT_COL_UPDATE_SINGLE(page)					\
	WT_COL_UPDATE_SLOT(page, 0)

/*
 * WT_COL_APPEND is a single WT_INSERT list, used for fixed- and variable-length
 * appends.
 */
#define	WT_COL_APPEND(page)						\
	((page)->u.col_leaf.append == NULL ?				\
	    NULL : (page)->u.col_leaf.append[0])

/* WT_FIX_FOREACH walks fixed-length bit-fields on a disk page. */
#define	WT_FIX_FOREACH(btree, dsk, v, i)				\
	for ((i) = 0,							\
	    (v) = (i) < (dsk)->u.entries ?				\
	    __bit_getv(WT_PAGE_DISK_BYTE(dsk), 0, (btree)->bitcnt) : 0;	\
	    (i) < (dsk)->u.entries; ++(i),				\
	    (v) = __bit_getv(WT_PAGE_DISK_BYTE(dsk), i, (btree)->bitcnt))

/*
 * WT_OFF_FOREACH --
 *	Walks WT_OFF/WT_OFF_RECORD references on a page, incrementing a pointer
 *	based on its declared type.
 */
#define	WT_OFF_FOREACH(dsk, offp, i)					\
	for ((offp) = WT_PAGE_DISK_BYTE(dsk),				\
	    (i) = (dsk)->u.entries; (i) > 0; ++(offp), --(i))
