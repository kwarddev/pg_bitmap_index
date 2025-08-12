#include "postgres.h"

#include "access/amapi.h"
#include "access/generic_xlog.h"
#include "access/reloptions.h"
#include "commands/vacuum.h"
#include "storage/bufmgr.h"
#include "storage/indexfsm.h"
#include "utils/memutils.h"
#include "access/tableam.h"
#include "utils/hsearch.h"

PG_MODULE_MAGIC;

static IndexBuildResult *pg_bitmap_build(Relation heap, Relation index, struct IndexInfo *indexInfo);
static bool pg_bitmap_validate(Oid opclassoid);
Datum pg_bitmap_index_handler(PG_FUNCTION_ARGS);

typedef struct BitmapEntry {
  Datum key;
  uint8 *bitmap;
} BitmapEntry;

typedef struct BitmapBuildState {
  HTAB *bitmap_hash;
  uint64 rownum;
  uint64 nrows;
} BitmapBuildState;

PG_FUNCTION_INFO_V1(pg_bitmap_index_handler);

Datum
pg_bitmap_index_handler(PG_FUNCTION_ARGS)
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

  amroutine->ambuild = pg_bitmap_build;
  amroutine->ambuildempty = NULL;
  amroutine->aminsert = NULL;
  amroutine->ambulkdelete = NULL;
  amroutine->amvacuumcleanup = NULL;
  amroutine->amcanreturn = NULL;
  amroutine->amoptions = NULL;
  amroutine->amvalidate = pg_bitmap_validate;
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
pg_bitmap_build_callback(Relation index,
                         ItemPointer tid,
                         Datum *values,
                         bool *isnull,
                         bool tupleIsAlive,
                         void *state)
{
  BitmapBuildState *buildstate = (BitmapBuildState *) state;
  HASHACTION action;
  BitmapEntry *entry;
  bool found;
  uint64 bitmap_size;
  uint64 byte_index;
  uint8 bit_mask;

  /* Skip null values for now */
  if (isnull[0])
    return;

  /* Initialize hash table on first call */
  if (buildstate->bitmap_hash == NULL)
  {
    HASHCTL hash_ctl;

    memset(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(Datum);
    hash_ctl.entrysize = sizeof(BitmapEntry);
    hash_ctl.hcxt = CurrentMemoryContext;

    buildstate->bitmap_hash = hash_create("Bitmap Build Hash",
                                          256,
                                          &hash_ctl,
                                          HASH_ELEM | HASH_CONTEXT);
  }

  /* Look up or create entry for this key value */
  action = HASH_ENTER;
  entry = (BitmapEntry *) hash_search(buildstate->bitmap_hash,
                                       &values[0],
                                       action,
                                       &found);

  /* If new entry, initialize bitmap */
  if (!found)
  {
    /* Calculate bitmap size (one bit per row, rounded up to bytes) */
    bitmap_size = (buildstate->nrows + 7) / 8;
    entry->key = values[0];
    entry->bitmap = (uint8 *) palloc0(bitmap_size);
  }

  /* Set the bit for this row in the bitmap */
  byte_index = buildstate->rownum / 8;
  bit_mask = 1 << (buildstate->rownum % 8);
  entry->bitmap[byte_index] |= bit_mask;

  buildstate->rownum++;
}

static IndexBuildResult *
pg_bitmap_build(Relation heap, Relation index, struct IndexInfo *indexInfo)
{
  IndexBuildResult *result;
	double reltuples;
	BitmapBuildState buildstate;
	uint64 total_rows;
	HASH_SEQ_STATUS hash_seq;
	BitmapEntry *entry;
	Buffer buffer;
	Page page;
	GenericXLogState *state;

	if (RelationGetNumberOfBlocks(index) != 0)
		elog(ERROR, "index \"%s\" already contains data",
			 RelationGetRelationName(index));

	/* Initialize the bitmap build state */
	memset(&buildstate, 0, sizeof(buildstate));
	buildstate.bitmap_hash = NULL;
	buildstate.rownum = 0;

	/* Get total number of rows for bitmap sizing */
	total_rows = RelationGetNumberOfBlocks(heap) * MaxHeapTuplesPerPage;
	buildstate.nrows = total_rows;

	/* Do the heap scan */
	reltuples = table_index_build_scan(heap, index, indexInfo, true, true,
									   pg_bitmap_build_callback, &buildstate,
									   NULL);

	/* Now write the bitmap data to the index pages */
	if (buildstate.bitmap_hash != NULL)
	{
		uint32 nentries = 0;

		/* Count entries */
		hash_seq_init(&hash_seq, buildstate.bitmap_hash);
		while ((entry = (BitmapEntry *) hash_seq_search(&hash_seq)) != NULL)
			nentries++;

		/* For now, just log the statistics */
		elog(INFO, "Bitmap index built: %u unique values, %.0f total rows",
			 nentries, reltuples);

		/* TODO: Write bitmap data to index pages */
		/* This would involve:
		 * 1. Creating index pages using GenericXLog
		 * 2. Writing bitmap entries to pages
		 * 3. Creating a metapage with index statistics
		 */

		/* Clean up hash table */
		hash_destroy(buildstate.bitmap_hash);
	}

	result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
	result->heap_tuples = reltuples;
	result->index_tuples = reltuples;  /* For bitmap index, this is the same */

	return result;
}

static bool
pg_bitmap_validate(Oid opclassoid)
{
    /* Optional: Add validation logic if needed */
    return true;
}