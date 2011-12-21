/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2008-2011 WiredTiger, Inc.
 *	All rights reserved.
 */

/*
 * In WiredTiger there are "file allocation units", which is the smallest file
 * chunk that can be allocated.  The smallest file allocation unit is 512B; the
 * largest is 128MB.  (The maximum of 128MB is enforced by the software, it
 * could be set as high as 4GB.)  Btree leaf and internal pages, as well as
 * overflow chunks, are allocated in groups of 1 or more allocation units.
 *
 * We use 32-bit unsigned integers to store file locations on file pages, and
 * all such file locations are counts of file allocation units.  In the code
 * these are called "addrs".  To simplify bookkeeping, page sizes must also be
 * a multiple of the allocation unit size.
 */
#define	WT_ADDR_INVALID		(UINT32_MAX)		/* Invalid address */

/*
 * The minimum maximum file size is 2TB (512B x 2^32), and the maximum maximum
 * file size is 2EB (512MB x 2^32).
 *
 * In summary, small file allocation units limit the file size and minimize
 * wasted space when storing overflow items; as the allocation unit grows,
 * the maximum size of the file grows as well.
 *
 * The minimum btree leaf and internal page sizes are 512B, the maximum 512MB.
 * (The maximum of 512MB is enforced by the software, it could be set as high
 * as 4GB.)
 *
 * Key and data item lengths are stored in 32-bit unsigned integers, meaning
 * the largest key or data item is 4GB (minus a few bytes).  Record numbers
 * are stored in 64-bit unsigned integers, meaning the largest record number
 * is "really, really big".
 */
#define	WT_BTREE_ALLOCATION_SIZE_MIN	(512)
#define	WT_BTREE_ALLOCATION_SIZE_MAX	(128 * WT_MEGABYTE)
#define	WT_BTREE_PAGE_SIZE_MAX		(512 * WT_MEGABYTE)

/* The file's description is written into the first 512B of the file. */
#define	WT_BTREE_DESC_SECTOR		512

/*
 * Limit the maximum size of a single object to 4GB - 512B: in some places we
 * allocate memory to store objects plus associated data structures, in other
 * places we need out-of-band values in object sizes.   512B is far more space
 * than we ever need, but I'm not eager to debug any off-by-ones, and storing
 * a 4GB object in the file is flatly insane, anyway.
 */
#define	WT_BTREE_OBJECT_SIZE_MAX	(UINT32_MAX - 512)

/*
 * Underneath the Btree code is the OS layer, where sizes are stored as numbers
 * of bytes.   In the OS layer, 32-bits is too small (a file might be larger
 * than 4GB), so we use a standard type known to hold the size of a file, off_t.
 *
 * The first 512B of the file hold the file's description.  Since I don't want
 * to give up a full allocation-size to the file's description, we offset addrs
 * by 512B.
 */
/* Convert a data address to/from a byte offset. */
#define	WT_ADDR_TO_OFF(btree, addr)					\
	(WT_BTREE_DESC_SECTOR + (off_t)(addr) * (off_t)(btree)->allocsize)
#define	WT_OFF_TO_ADDR(btree, off)					\
	((uint32_t)((off - WT_BTREE_DESC_SECTOR) / (btree)->allocsize))
#define	WT_FILE_OFF_MAX(btree)						\
	WT_ADDR_TO_OFF(btree, UINT32_MAX - 1)

/*
 * WT_BTREE_DESC --
 *	The file's description.
 */
struct __wt_btree_desc {
#define	WT_BTREE_MAGIC		120897
	uint32_t magic;			/* 00-03: Magic number */
#define	WT_BTREE_MAJOR_VERSION	0
	uint16_t majorv;		/* 04-05: Major version */
#define	WT_BTREE_MINOR_VERSION	1
	uint16_t minorv;		/* 06-07: Minor version */

	uint32_t cksum;			/* 08-11: Checksum */

	uint32_t free_addr;		/* 12-15: Free list page address */
	uint32_t free_size;		/* 16-19: Free list page length */

	uint32_t unused;		/* 20-23: Unused */

	/*
	 * We maintain page LSN's for the file in the non-transactional case
	 * (where, instead of a log reference, the LSN is simply a counter),
	 * as that's how salvage can determine the most recent page between
	 * pages overlapping the same key range.  This non-transactional LSN
	 * has to be persistent, and so it's included in the file's metadata.
	 */
	uint64_t lsn;			/* 24-31: Non-transactional page LSN */
};
/*
 * WT_BTREE_DESC_SIZE is the expected structure size -- we verify the build to
 * ensure the compiler hasn't inserted padding (padding won't cause failure,
 * since we reserve the first sector of the file for this information, but it
 * would be worth investigation, regardless).
 */
#define	WT_BTREE_DESC_SIZE		32

/*
 * WT_DISK_REQUIRED--
 *	Return bytes needed for byte length, rounded to an allocation unit.
 */
#define	WT_DISK_REQUIRED(session, size)					\
	(WT_ALIGN((size) + WT_PAGE_DISK_SIZE, (session)->btree->allocsize))

/*
 * Don't compress the first 32B of the block (almost all of the WT_PAGE_DISK
 * structure) because we need the block's checksum and on-disk and in-memory
 * sizes to be immediately available without decompression (the checksum and
 * the on-disk block sizes are used during salvage to figure out where the
 * blocks are, and the in-memory page size tells us how large a buffer we need
 * to decompress the file block.  We could take less than 32B, but a 32B
 * boundary is probably better alignment for the underlying compression engine,
 * and skipping 32B won't matter in terms of compression efficiency.
 */
#define	WT_COMPRESS_SKIP    32

/*
 * WT_PAGE_DISK --
 *
 * All on-disk pages have a common header, defined by the WT_PAGE_DISK
 * structure.  The header has no version number or mode bits, and the page type
 * and/or flags value will have to be modified when changes are made to the page
 * layout.  (The page type appears early in the header to make this simpler.)
 * In other words, the page type declares the contents of the page and how to
 * read it.
 */
struct __wt_page_disk {
	/*
	 * The record number of the first record of the page is stored on disk
	 * so we can figure out where the column-store leaf page fits into the
	 * key space during salvage.
	 */
	uint64_t recno;			/* 00-07: column-store starting recno */

	/*
	 * The LSN is a 64-bit chunk to make assignment and comparisons easier,
	 * but it's 2 32-bit values underneath: a file number and a file offset.
	 */
#define	WT_LSN_FILE(lsn)						\
	((uint32_t)(((lsn) & 0xffffffff00000000ULL) >> 32))
#define	WT_LSN_OFFSET(lsn)						\
	((uint32_t)((lsn) & 0xffffffff))
#define	WT_LSN_INCR(lsn)						\
	(++(lsn))
	uint64_t lsn;			/* 08-15: LSN file/offset pair */

	uint32_t cksum;			/* 16-19: checksum */

	/*
	 * We write the page size in the on-disk page header because it makes
	 * salvage easier.  (If we don't know the expected page length, we'd
	 * have to read increasingly larger chunks from the file until we find
	 * one that checksums, and that's going to be harsh given WiredTiger's
	 * large page sizes.)
	 *
	 * We also store an in-memory size because otherwise we'd have no idea
	 * how much memory to allocate in order to expand a compressed page.
	 */
	uint32_t size;			/* 20-23: on-disk page size */
	uint32_t memsize;		/* 24-27: in-memory page size */

	union {
		uint32_t entries;	/* 28-31: number of cells on page */
		uint32_t datalen;	/* 28-31: overflow data length */
	} u;

	uint8_t type;			/* 32: page type */

	/*
	 * End the the WT_PAGE_DISK structure with 3 bytes of padding: it wastes
	 * space, but it leaves the WT_PAGE_DISK structure 32-bit aligned and
	 * having a small amount of space to play with in the future can't hurt.
	 */
	uint8_t unused[3];		/* 33-35: unused padding */
};
/*
 * WT_PAGE_DISK_SIZE is the expected structure size -- we verify the build to
 * ensure the compiler hasn't inserted padding (which would break the world).
 */
#define	WT_PAGE_DISK_SIZE		36

/*
 * WT_PAGE_DISK_BYTE --
 *	The first usable data byte on the page (past the header).
 */
#define	WT_PAGE_DISK_BYTE(dsk)						\
	((void *)((uint8_t *)(dsk) + WT_PAGE_DISK_SIZE))

/*
 * WT_DISK_OFFSET, WT_REF_OFFSET --
 *	Return the offset/pointer of a pointer/offset in a page disk image.
 */
#define	WT_DISK_OFFSET(dsk, p)						\
	WT_PTRDIFF32(p, dsk)
#define	WT_REF_OFFSET(page, o)						\
	((void *)((uint8_t *)((page)->dsk) + (o)))
