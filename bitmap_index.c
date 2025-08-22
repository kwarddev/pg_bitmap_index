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
}

/*
 * pg_bitmap_build() -- build a new bitmap index.
 */
static IndexBuildResult *
pg_bitmap_build(Relation heap, Relation index, struct IndexInfo *indexInfo)
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






	return result;
}

static bool
pg_bitmap_validate(Oid opclassoid)
{
    /* Optional: Add validation logic if needed */
    return true;
}