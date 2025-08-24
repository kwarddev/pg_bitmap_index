EXTENSION = bitmap_index
MODULE_big = bitmap_index
OBJS = bminsert.o bmutils.o
DATA = bitmap_index--0.1.sql

PG_CONFIG = /opt/homebrew/opt/postgresql@14/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)