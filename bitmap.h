/*-------------------------------------------------------------------------
 *
 * bitmap.h
 *	  Header for bitmap index.
 *
 *-------------------------------------------------------------------------
 */
#ifndef _BITMAP_INDEX_H_
#define _BITMAP_INDEX_H_

#define BITMAP_METAPAGE_BLKNO 0

#include "postgres.h"

#include "access/amapi.h"
#include "utils/hsearch.h"
#include "storage/bufpage.h"
#include "storage/relfilenode.h"
#include "utils/relcache.h"
#include "utils/memutils.h"
#include "nodes/tidbitmap.h"

/* Bitmap page flags */
#define BITMAP_META    (1<<0)
#define BITMAP_DELETED (1<<1)
#define BITMAP_DATA    (1<<2)

/*
 * The page ID is for the convenience of pg_filedump and similar utilities,
 * which otherwise would have a hard time telling pages of different index
 * types apart.  It should be the last 2 bytes on the page.  This is more or
 * less "free" due to alignment considerations.
 *
 * See comments above GinPageOpaqueData.
 */
#define BITMAP_PAGE_ID 0xFF84

/* Macros for accessing bitmap page structures */
#define BitmapPageGetOpaque(page) ((BitmapPageOpaque) PageGetSpecialPointer(page))

/* Bitmap index options */
typedef struct BitmapOptions {
  int32 vl_len_; /* varlena header (do not touch directly!) */
  int maxDistinctValues; /* stop indexing after N distinct values */
  // int minRowsPerValue; /* don't index values with less than N rows */
  // bool compressSparseBitmaps; /* compress if density < threshold */
  // float compressionThreshold; /* e.g., 0.1 = compress if < 10% bits set */
} BitmapOptions;

/* Metadata of bitmap index */
typedef struct BitmapMetaPageData {
  uint32 magicNumber;
  uint32 version;            /* index version */
  uint32 nDistinctValues;    /* current number of distinct values */
  uint64 nTuples;           /* number of rows in table */
  BitmapOptions opts;        /* frozen options */
  /* Maybe add later:
  BlockNumber lastDataPage;
  uint32 totalPages; */

} BitmapMetaPageData;

#define BITMAP_MAGIC_NUMBER (0xB17BA9ED)

typedef struct BitmapPageOpaqueData {
  uint16 flags;          /* BITMAP_META, BITMAP_DATA, etc */
  uint16 bitmap_page_id; /* BITMAP_PAGE_ID for identification */
} BitmapPageOpaqueData;

typedef BitmapPageOpaqueData *BitmapPageOpaque;

#define BitmapPageGetMeta(page) ((BitmapMetaPageData *) PageGetContents(page))
typedef struct BitmapEntry {
  Datum key;                  /* the distinct value */
  TIDBitmap *tidbitmap;       /* TIDBitmap for this value */
} BitmapEntry;

typedef struct BitmapState {
  HTAB *bmapHash;
  MemoryContext tmpCtx;
  int64 indtuples;
  uint64 nrows;
  int nvalues;
  int maxValues;
} BitmapState;

typedef struct
{
	BitmapState	bmapstate;
} BitmapBuildState;

/* bitmaputils.c */
extern void BitmapFillMetaPage(Relation index, Page metaPage);
extern void BitmapInitMetaPage(Relation index, ForkNumber forknum);
extern void BitmapInitPage(Page page, uint16 flags);
extern void initBitmapState(BitmapState *state, Relation index);

#endif
