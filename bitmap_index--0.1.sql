-- Creates the index access method
CREATE FUNCTION bitmap_index_handler(internal)
  RETURNS index_am_handler
  AS 'MODULE_PATHNAME'
  LANGUAGE C;

CREATE ACCESS METHOD bitmap_index TYPE INDEX HANDLER bitmap_index_handler;

CREATE OPERATOR CLASS bitmap_int_ops
  DEFAULT FOR TYPE integer USING bitmap_index AS
  STORAGE integer;
