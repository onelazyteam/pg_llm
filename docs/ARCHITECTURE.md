# pg_llm Architecture (v1.1)

## 1. Overview

`pg_llm` is a PostgreSQL extension that provides LLM capabilities as native SQL functions. The extension keeps state and artifacts in `_pg_llm_catalog`, while model invocation and orchestration run inside PostgreSQL backends.

The architecture is designed around:

- Backward-compatible text APIs
- Additive structured `jsonb` and SRF streaming APIs
- Persistent session/audit/trace/report/knowledge data
- Configurable confidence routing and local fallback
- In-database observability and security controls

## 2. High-Level Components

### 2.1 SQL API Layer (`src/pg_llm.cpp`)

Main responsibilities:

- SQL entrypoints (`PG_FUNCTION_INFO_V1`) and argument decoding
- SPI-based reads/writes to `_pg_llm_catalog`
- Response shaping (`text`, `jsonb`, SRF rows)
- Request-level audit and trace persistence

### 2.2 Model Layer (`src/models/*`)

- `LLMInterface`: provider adapter for chat, streaming, and embeddings
- `ModelManager`: model registration, lazy instance loading, parallel inference
- Decrypts encrypted model secrets when loading model instances
- Includes deterministic mock provider path for offline tests

### 2.3 Text2SQL Layer (`src/text2sql/*`)

- Schema discovery / optional caller-supplied schema
- Optional vector retrieval for relevant examples/context
- SQL statement generation (`generate_statement`)
- SQL execution + EXPLAIN capture for structured outputs

### 2.4 Support Layer (`src/utils/*`)

- Core GUC definitions
- JSON/UUID helpers
- AES-GCM encryption and decryption for secrets
- Redaction utilities for audit/trace metadata
- PostgreSQL-native logging wrapper with glog-compatible GUCs

## 3. Persistent Catalog Model

All extension-owned data is persisted in `_pg_llm_catalog`.

Core tables:

- `pg_llm_models`: model routing metadata and encrypted secrets
- `pg_llm_sessions`, `pg_llm_session_messages`: session state and history
- `pg_llm_audit_log`: request-level audit events
- `pg_llm_trace_log`: intermediate decisions and pipeline traces
- `pg_llm_reports`: persisted report artifacts
- `pg_llm_knowledge_documents`, `pg_llm_knowledge_chunks`: RAG corpus
- `pg_llm_feedback`: user feedback linked to `request_id`
- `pg_llm_queries`, `pg_llm_vectors`: text2sql/vector support data

## 4. Public API Shape

### 4.1 Backward-Compatible Text APIs

- `pg_llm_chat`
- `pg_llm_parallel_chat`
- `pg_llm_text2sql`
- Existing session APIs (`pg_llm_create_session`, `pg_llm_multi_turn_chat`, etc.)

These remain available and act as thin wrappers over structured execution paths.

### 4.2 Structured APIs

- `pg_llm_chat_json`, `pg_llm_parallel_chat_json`, `pg_llm_text2sql_json`
- `pg_llm_execute_sql_with_analysis`, `pg_llm_generate_report`
- `pg_llm_get_session`, `pg_llm_get_session_messages`, `pg_llm_update_session_state`, `pg_llm_delete_session`
- `pg_llm_add_knowledge`, `pg_llm_search_knowledge`
- `pg_llm_record_feedback`
- `pg_llm_get_audit_log`, `pg_llm_get_trace`

### 4.3 Streaming APIs

- `pg_llm_chat_stream`
- `pg_llm_multi_turn_chat_stream`

Both return SRF rows with `seq_no`, `chunk`, `is_final`, `model_name`, `confidence_score`, `request_id`.

## 5. Runtime Flows

### 5.1 Chat / Multi-Turn Chat

1. Resolve model instance from `ModelManager`.
2. Optionally assemble RAG context (`options.enable_rag`).
3. Invoke model (blocking or streaming).
4. Apply confidence threshold logic and optional local fallback.
5. Persist session messages (for multi-turn mode).
6. Persist audit and trace records.

### 5.2 Parallel Chat and Routing

1. Run candidate models in parallel.
2. Select the highest-confidence candidate.
3. Evaluate effective threshold (model-level / GUC / options).
4. Trigger fallback model when confidence is below threshold.
5. Persist candidate scores and routing decisions to trace/audit.

### 5.3 Text2SQL

1. Build schema context.
2. Perform optional vector retrieval and similar-query lookup.
3. Generate SQL through LLM prompt path.
4. Execute SQL via SPI and capture tabular result.
5. Run `EXPLAIN` and include plan lines.
6. Persist trace/audit.

### 5.4 Reports

1. Execute SQL and capture structured result + explain output.
2. Ask model for narrative summary.
3. Produce report JSON with recommendations and Vega-Lite spec.
4. Persist report artifact in catalog.

### 5.5 Knowledge / Feedback

- Knowledge ingestion stores document, chunks, deterministic embeddings.
- Knowledge search ranks chunks by vector distance + lexical boost.
- Feedback is stored per `request_id` for later workflow use.

## 6. Security And Observability

### 6.1 Core GUCs

- `pg_llm.master_key`
- `pg_llm.audit_enabled`
- `pg_llm.trace_enabled`
- `pg_llm.redact_sensitive`
- `pg_llm.audit_sample_rate`
- `pg_llm.default_confidence_threshold`
- `pg_llm.default_local_fallback`

### 6.2 Secret Handling

- API keys and secret-bearing configs are encrypted before persistence.
- Decryption happens in backend memory only.
- Redaction is applied in audit/trace output when enabled.

### 6.3 Observability

- `pg_llm_audit_log` captures request outcome and metadata.
- `pg_llm_trace_log` captures intermediate execution decisions.
- `request_id` is the correlation key across APIs and tables.

## 7. Build And Packaging

- Extension version: `1.1`
- Upgrade path: `pg_llm--1.0--1.1.sql`
- Primary build/install path: CMake (`contrib/pg_llm/CMakeLists.txt`)
- SQL regression tests are maintained in `test/sql` and `test/expected`
