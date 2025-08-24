/*-------------------------------------------------------------------------
 *
 * bitmaputils.c
 *		Bitmap index utilities.
 *
 *-------------------------------------------------------------------------
 */

#include "bitmap.h"
#include "access/generic_xlog.h"
#include "storage/bufmgr.h"

/*
 * Initialize any page of a bitmap index
 */
void
BitmapInitPage(Page page, uint16 flags)
{
  BitmapPageOpaque opaque;

  PageInit(page, BLCKSZ, sizeof(BitmapPageOpaqueData));

  opaque = BitmapPageGetOpaque(page);
  opaque->flags = flags;
  opaque->bitmap_page_id = BITMAP_PAGE_ID;
}

/*
 * Construct a default set of Bitmap options.
 */
static BitmapOptions *
makeDefaultBitmapOptions(void)
{
  BitmapOptions *opts;

  opts = (BitmapOptions *) palloc0(sizeof(BitmapOptions));
  opts->maxDistinctValues = 10;

  SET_VARSIZE(opts, sizeof(BitmapOptions));

  return opts;
}

/*
 * Fill in in blank metaPage with metadata values
 */
void
BitmapFillMetapage(Relation index, Page metaPage) {
  BitmapOptions *opts;
  BitmapMetaPageData *metadata;

  opts = (BitmapOptions *) index->rd_options;
  if (!opts)
    opts = makeDefaultBitmapOptions();

  /*
   * Initialize contents of meta page, including a copy of the options,
   * which are now frozen for the life of the index.
   */
  BitmapInitPage(metaPage, BITMAP_META);
  metadata = BitmapPageGetMeta(metaPage);
  memset(metadata, 0, sizeof(BitmapMetaPageData));  /* Zero everything first */
  metadata->magicNumber = BITMAP_MAGIC_NUMBER;
  metadata->version = 1;
  metadata->nDistinctValues = 0;
  metadata->nTuples = 0;
  metadata->opts = *opts;

  ((PageHeader) metaPage)->pd_lower += sizeof(BitmapMetaPageData);

  /* If this fails, BitmapMetaPageData is probably too large for a page */
  Assert(((PageHeader) metaPage)->pd_lower <= ((PageHeader) metaPage)-> pd_upper);
}

void
initBitmapState(BitmapState *state, Relation index)
{
  HASHCTL ctl;
  Buffer metaBuffer;
  BitmapMetaPageData *meta;
  Page metaPage;
  BitmapOptions *opts;

  state->tmpCtx = AllocSetContextCreate(CurrentMemoryContext,
                                        "Bitmap build temporary context",
                                        ALLOCSET_DEFAULT_SIZES);

  metaBuffer = ReadBuffer(index, BITMAP_METAPAGE_BLKNO);
  LockBuffer(metaBuffer, BUFFER_LOCK_SHARE);
  metaPage = BufferGetPage(metaBuffer);
  meta = BitmapPageGetMeta(metaPage);
  opts = &meta->opts;
  state->maxValues = opts->maxDistinctValues;
  UnlockReleaseBuffer(metaBuffer);

  memset(&ctl, 0, sizeof(HASHCTL));
  ctl.keysize = sizeof(Datum);
  ctl.entrysize = sizeof(BitmapEntry);
  ctl.hcxt = state->tmpCtx;

  state->bmapHash = hash_create("Bitmap value hash",
                                state->maxValues,
                                &ctl,
                                HASH_ELEM | HASH_CONTEXT);
  state->indtuples = 0;
  state->nrows = 0;
  state->nvalues = 0;
}

void
BitmapInitMetapage(Relation index, ForkNumber forknum) {
  // slot # of the buffer array where my meta page is
  Buffer metaBuffer;
  // new 8KB page for the index
  Page metaPage;
  // WAL Log state to log this page before it's flushed to disk
  GenericXLogState *state;

  // extends the file (index) from 0KB to 8KB, P_NEW tells it the page is new
  // zeroes out the new page.
  metaBuffer = ReadBufferExtended(index, forknum, P_NEW, RBM_NORMAL, NULL);
  LockBuffer(metaBuffer, BUFFER_LOCK_EXCLUSIVE);
  Assert(BufferGetBlockNumber(metaBuffer) == BITMAP_METAPAGE_BLKNO);

  state = GenericXLogStart(index);
  metaPage = GenericXLogRegisterBuffer(state, metaBuffer, GENERIC_XLOG_FULL_IMAGE);
  BitmapFillMetapage(index, metaPage);
  GenericXLogFinish(state);

  UnlockReleaseBuffer(metaBuffer);
}
