# pg_llm Documentation Index

This folder contains implementation-aligned design docs for `pg_llm` extension version `1.1`.

## Scope

`pg_llm` is a PostgreSQL extension that exposes LLM workflows as SQL APIs:

- Model registry and runtime routing
- Single-turn, multi-turn, parallel and streaming chat
- Structured JSON APIs for observability and automation
- Text2SQL with optional vector/RAG context
- SQL execution analysis and persisted report generation
- Knowledge ingestion/retrieval and feedback persistence
- Audit and trace logs correlated by `request_id`

## Build And Install (CMake)

`pg_llm` is compiled with CMake.

```bash
cd contrib/pg_llm
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
sudo cmake --install .
```

Then in PostgreSQL:

```sql
CREATE EXTENSION vector;
CREATE EXTENSION pg_llm;
```

## Test Notes

Regression SQL tests are kept under `test/sql` and can be executed with `installcheck` from the extension directory when PGXS test environment is available.

## Design Docs

- [ARCHITECTURE.md](ARCHITECTURE.md): system architecture, catalogs, execution flow, security and observability
- [ARCHITECTURE.zh.md](ARCHITECTURE.zh.md): 中文架构说明
- [text2sql_design.md](text2sql_design.md): Text2SQL pipeline and API contract
- [text2sql_design.zh.md](text2sql_design.zh.md): 中文 Text2SQL 设计
- [glog_integration.md](glog_integration.md): logging compatibility layer and migration notes
- [glog_integration.zh.md](glog_integration.zh.md): 中文日志兼容层说明
