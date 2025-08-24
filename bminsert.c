#include "bitmap.h"
#include "access/tableam.h"
#include "storage/bufmgr.h"
#include "utils/rel.h"
#include "access/relscan.h"
#include "nodes/tidbitmap.h"
#include "miscadmin.h"
#include "lib/stringinfo.h"

PG_MODULE_MAGIC;

static IndexBuildResult *bmapbuild(Relation heap, Relation index, struct IndexInfo *indexInfo);
static bool bitmap_index_validate(Oid opclassoid);
void BitmapInitMetapage(Relation index, ForkNumber forknum);
Datum bitmap_index_handler(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(bitmap_index_handler);

Datum
bitmap_index_handler(PG_FUNCTION_ARGS)
{
  IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

  amroutine->amstrategies = 0;
  amroutine->amsupport = 0;
  amroutine->amcanorder = false;
  amroutine->amcanunique = false;
  amroutine->amcanmulticol = false;
  amroutine->amoptionalkey = false;
  amroutine->amsearcharray = false;
  amroutine->amsearchnulls = false;
  amroutine->amgettuple = NULL;
  amroutine->amgetbitmap = NULL;
  amroutine->amcanparallel = false;
  amroutine->amcaninclude = false;

  amroutine->ambuild = bmapbuild;
  amroutine->ambuildempty = NULL;
  amroutine->aminsert = NULL;
  amroutine->ambulkdelete = NULL;
  amroutine->amvacuumcleanup = NULL;
  amroutine->amcanreturn = NULL;
  amroutine->amoptions = NULL;
  amroutine->amvalidate = bitmap_index_validate;
  amroutine->ambeginscan = NULL;
  amroutine->amrescan = NULL;
  amroutine->amgettuple = NULL;
  amroutine->amgetbitmap = NULL;
  amroutine->amendscan = NULL;
  amroutine->ammarkpos = NULL;
  amroutine->amrestrpos = NULL;

  PG_RETURN_POINTER(amroutine);
}

static void
bitmapBuildCallback(Relation index,
                  ItemPointer tid,
                  Datum *values,
                  bool *isnull,
                  bool tupleIsAlive,
                  void *state)
{
  BitmapBuildState *buildstate = (BitmapBuildState *) state;
  BitmapState *bstate = &buildstate->bmapstate;

  // For single-column index:
  Datum key = values[0];
  bool found;
  BitmapEntry *entry;

  // Find or create entry for this value
  entry = (BitmapEntry *) hash_search(bstate->bmapHash, &key, HASH_ENTER, &found);

  if (!found)
  {
    // Initialize new entry with a TIDBitmap
    entry->key = key;
    // Create a TIDBitmap for this value
    // Using work_mem size for max bytes (typically 4MB)
    // TODO: We need to dynamically extend this
    entry->tidbitmap = tbm_create(work_mem * 1024L, NULL);
    bstate->nvalues++;
  }

  // Add this TID to the bitmap for this value
  tbm_add_tuples(entry->tidbitmap, tid, 1, false);

  bstate->indtuples++;
  bstate->nrows++;
}

/*
 * bmapbuild() -- build a new bitmap index.
 */
static IndexBuildResult *
bmapbuild(Relation heap, Relation index, struct IndexInfo *indexInfo)
{
  IndexBuildResult *result;
	double reltuples;
	BitmapBuildState buildstate;

  /*
   * We expect to be called exactly once for any index relation.
   */
	if (RelationGetNumberOfBlocks(index) != 0)
		elog(ERROR, "index \"%s\" already contains data",
			 RelationGetRelationName(index));

  BitmapInitMetapage(index, MAIN_FORKNUM);

  /* Initialize the bitmap build state */
  memset(&buildstate, 0, sizeof(buildstate));
  initBitmapState(&buildstate.bmapstate, index);

	reltuples = table_index_build_scan(heap, index, indexInfo, true, true,
									   bitmapBuildCallback, &buildstate,
									   NULL);

  // Debug: Print bitmap contents before serialization
  elog(INFO, "Bitmap index build complete: %d distinct values, %ld tuples",
       buildstate.bmapstate.nvalues, buildstate.bmapstate.indtuples);

  // Print each distinct value and its bitmap
  if (buildstate.bmapstate.bmapHash)
  {
    HASH_SEQ_STATUS hash_seq;
    BitmapEntry *entry;
    int value_count = 0;

    hash_seq_init(&hash_seq, buildstate.bmapstate.bmapHash);
    while ((entry = hash_seq_search(&hash_seq)) != NULL && value_count < 10) // Limit output for readability
    {
      TBMIterator *iterator;
      TBMIterateResult *tbmres;
      int tid_count = 0;
      StringInfoData tidlist;

      initStringInfo(&tidlist);

      // Get statistics about this bitmap
      iterator = tbm_begin_iterate(entry->tidbitmap);
      while ((tbmres = tbm_iterate(iterator)) != NULL)
      {
        // For each page in the bitmap
        for (int i = 0; i < tbmres->ntuples; i++)
        {
          if (tid_count < 20) // Show first 20 TIDs
          {
            if (tid_count > 0)
              appendStringInfo(&tidlist, ", ");
            appendStringInfo(&tidlist, "(%u,%d)",
                           tbmres->blockno,
                           tbmres->offsets[i]);
          }
          tid_count++;
        }
      }
      tbm_end_iterate(iterator);

      // For debugging, just print the datum pointer value
      // TODO: Add proper type handling for different column types
      elog(INFO, "Value (Datum %p): %d TIDs - [%s%s]",
           (void*)entry->key,
           tid_count,
           tidlist.data,
           tid_count > 20 ? ", ..." : "");

      pfree(tidlist.data);
      value_count++;
    }

    if (buildstate.bmapstate.nvalues > 10)
      elog(INFO, "... and %d more distinct values",
           buildstate.bmapstate.nvalues - 10);
  }

  // TODO: Serialize TIDBitmaps to disk
  // For each entry in the hash table:
  // 1. Iterate through the TIDBitmap to get all TIDs
  // 2. Convert to a compact on-disk format
  // 3. Write to index pages
  // For now, we'll just clean up and return results

  // Clean up TIDBitmaps in hash table
  if (buildstate.bmapstate.bmapHash)
  {
    HASH_SEQ_STATUS hash_seq;
    BitmapEntry *entry;

    hash_seq_init(&hash_seq, buildstate.bmapstate.bmapHash);
    while ((entry = hash_seq_search(&hash_seq)) != NULL)
    {
      if (entry->tidbitmap)
        tbm_free(entry->tidbitmap);
    }
  }

  result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
  result->heap_tuples = reltuples;
  result->index_tuples = buildstate.bmapstate.indtuples;

  // Clean up temporary context
  if (buildstate.bmapstate.tmpCtx)
    MemoryContextDelete(buildstate.bmapstate.tmpCtx);

  return result;
}

static bool
bitmap_index_validate(Oid opclassoid)
{
    /* Optional: Add validation logic if needed */
    return true;
}
