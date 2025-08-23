EXTENSION = bitmap_index
MODULES = bitmap_index
DATA = bitmap_index--0.1.sql

PG_CONFIG = /opt/homebrew/opt/postgresql@14/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)