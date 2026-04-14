# Development Guide

This document describes how to build, test, and contribute to `pg_llm`.

## Code Layout

```text
pg_llm/
├── include/              # Public headers
├── src/                  # Extension source files
├── sql/                  # Extension SQL and upgrade scripts
├── test/                 # Regression tests
├── docs/                 # Design docs
├── CMakeLists.txt        # CMake build config
├── Makefile              # PGXS build/test entry
├── pg_llm.control        # Extension metadata
└── README.md
```

## Dependencies

Required:

- CMake 3.15+
- PostgreSQL 14+
- C++20 compiler
- OpenSSL
- libcurl
- JsonCpp
- pkg-config

## Build (CMake)

```bash
cd pg_llm
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
sudo cmake --install .
```

Build types:

- `Debug`
- `Release`
- `ASan`

## Enable Extension

```sql
CREATE EXTENSION vector;
CREATE EXTENSION pg_llm;
```

## Run Regression Tests

```bash
cd pg_llm
PG_CONFIG=/path/to/pg_config PGPORT=<port> make installcheck
```

Tests are located under:

- `test/sql`
- `test/expected`

## Logging

`pg_llm` uses PostgreSQL native logging via `include/utils/pg_llm_log.h`.

## Development Workflow

1. Create a feature/fix branch.
2. Implement code and tests.
3. Update docs when behavior changes.
4. Run build + `installcheck`.
5. Submit PR.

## Commit Convention

Each commit message must follow:

```text
[type] #number message
```

Allowed `type` values:

- `feature`
- `bug`
- `doc`
- `task`

Examples:

- `[feature] #123 Add confidence fallback routing`
- `[bug] #456 Fix SQL volatility mismatch`
- `[doc] #789 Update API docs`
- `[task] #321 Refactor model loading`

Enable hooks in the repository:

```bash
git config core.hooksPath .githooks
```
