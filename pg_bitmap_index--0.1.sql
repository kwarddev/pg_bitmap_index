-- Creates the index access method
CREATE FUNCTION pg_bitmap_index_handler(internal)
  RETURNS index_am_handler
  AS 'MODULE_PATHNAME'
  LANGUAGE C;

CREATE ACCESS METHOD pg_bitmap_index TYPE INDEX HANDLER pg_bitmap_index_handler;

CREATE OPERATOR CLASS pg_bitmap_int_ops
  DEFAULT FOR TYPE integer USING pg_bitmap_index AS
  STORAGE integer;
