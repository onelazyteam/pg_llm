TESTS = test_pg_llm.sql

REGRESS = $(patsubst %.sql,%,$(TESTS))
REGRESS_OPTS = --dbname=contrib_regression

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Clean target
clean:
	rm -f regression.* 