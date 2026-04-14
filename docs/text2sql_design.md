# Text2SQL Design (v1.1)

## 1. Goal

`pg_llm` Text2SQL turns natural language prompts into executable SQL inside PostgreSQL, with optional vector/RAG context and structured observability.

Primary APIs:

- `pg_llm_text2sql(...) returns text` (compatibility wrapper)
- `pg_llm_text2sql_json(...) returns jsonb` (authoritative structured output)

## 2. Pipeline

Implementation entrypoint: `build_text2sql_json_internal(...)` in `src/pg_llm.cpp`.

### Step 1: Context Assembly

- Resolve model instance from `ModelManager`.
- Parse runtime options into `Text2SQLConfig`.
- Build schema context:
  - caller-provided `schema_info`, or
  - schema introspection from `Text2SQL::get_schema_info()`.

### Step 2: Optional Vector Retrieval

When `use_vector_search = true`:

- Retrieve relevant schema/vector hits through `Text2SQL::search_vectors(prompt)`.
- Retrieve similar NL-SQL examples via `Text2SQL::get_similar_queries(prompt)`.

Vector storage/search uses `_pg_llm_catalog.pg_llm_vectors` with corrected column naming:

- `query_vector` (embedding column)
- `table_name`, `column_name`, `row_id`, `metadata`

### Step 3: SQL Generation

- Call `Text2SQL::generate_statement(prompt, schema, vector_hits, similar_queries)`.
- Output is normalized SQL text (`generated_sql`).

### Step 4: SQL Execution + Analysis

- Execute generated SQL through SPI.
- Return execution status, affected row count, and selected rows/columns.
- Run `EXPLAIN <sql>` and return plan lines in `execution.explain`.

### Step 5: Observability

- Persist audit event (`event_type = text2sql`) in `pg_llm_audit_log`.
- Persist trace payload in `pg_llm_trace_log`.
- Use `request_id` for correlation.

## 3. Input Contract

`pg_llm_text2sql_json(instance_name, prompt, schema_info, use_vector_search, options)`

- `instance_name text`: registered model instance
- `prompt text`: natural language query
- `schema_info text default NULL`: optional schema JSON override
- `use_vector_search bool default true`: toggle vector enhancement
- `options jsonb`: runtime tuning

Current option keys parsed by implementation:

- `enable_cache`
- `cache_ttl_seconds`
- `parallel_processing`
- `max_parallel_threads`
- `similarity_threshold`
- `sample_data_limit`

## 4. Output Contract (`jsonb`)

Typical shape:

```json
{
  "request_id": "uuid",
  "instance_name": "model-instance",
  "prompt": "show latest 10 orders",
  "generated_sql": "SELECT ...",
  "execution": {
    "status": "SPI_OK_SELECT",
    "row_count": 10,
    "columns": ["id", "created_at"],
    "rows": [{"id": "1", "created_at": "..."}],
    "explain": ["Seq Scan on ..."]
  },
  "similar_queries": ["..."],
  "vector_hits": [
    {
      "table_name": "orders",
      "column_name": "note",
      "row_id": 123,
      "similarity": 0.91,
      "metadata": {"source": "..."}
    }
  ]
}
```

`pg_llm_text2sql(...)` returns only `generated_sql` for compatibility.

## 5. Failure Semantics

Errors are raised as PostgreSQL errors (`ereport(ERROR)`) for:

- missing/invalid model instance
- SQL execution failures
- SPI errors
- malformed JSON input (`schema_info`/`options`)

## 6. Security Considerations

- Generated SQL is executed in the caller's PostgreSQL security context.
- Production usage should combine role permissions and schema-level controls.
- Trace/audit payloads follow `pg_llm.redact_sensitive` behavior.

## 7. Build And Deployment Notes

- Compiled and installed via CMake (`contrib/pg_llm/CMakeLists.txt`).
- Extension SQL definitions are versioned in `sql/pg_llm--1.1.sql` and upgrade script `sql/pg_llm--1.0--1.1.sql`.
