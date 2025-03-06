MODULES = pg_llm
EXTENSION = pg_llm
DATA = pg_llm--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Add source files
OBJS = src/visualization.o src/chat.o src/security.o src/sql_optimizer.o \
       src/report_generator.o src/hybrid_reasoning.o src/model_interface.o \
       src/conversation.o src/models/common_model.o src/models/chatgpt.o \
       src/models/qianwen.o src/models/wenxin.o src/models/doubao.o \
       src/models/deepseek.o src/models/local_model.o

# Add compilation options
CFLAGS += -I$(shell $(PG_CONFIG) --includedir)
CFLAGS += -I$(shell $(PG_CONFIG) --includedir-server)

# Add linking options
SHLIB_LINK += -lcurl -ljson-c

PG_CPPFLAGS = -I$(libpq_srcdir)

# Test configuration
REGRESS = tests/001_basic \
          tests/002_models \
          tests/003_chat \
          tests/test_report_visualization

# Test data directory
REGRESS_OPTS = --inputdir=./tests --outputdir=./tests/expected 