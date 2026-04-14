-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pg_llm UPDATE TO '1.1'" to load this file. \quit

ALTER TABLE _pg_llm_catalog.pg_llm_models
  ADD COLUMN IF NOT EXISTS encrypted_api_key text NOT NULL DEFAULT '',
  ADD COLUMN IF NOT EXISTS encrypted_config text NOT NULL DEFAULT '',
  ADD COLUMN IF NOT EXISTS confidence_threshold double precision NOT NULL DEFAULT 0,
  ADD COLUMN IF NOT EXISTS fallback_instance text NOT NULL DEFAULT '',
  ADD COLUMN IF NOT EXISTS is_local_fallback boolean NOT NULL DEFAULT false,
  ADD COLUMN IF NOT EXISTS capabilities jsonb NOT NULL DEFAULT '{}'::jsonb,
  ADD COLUMN IF NOT EXISTS created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
  ADD COLUMN IF NOT EXISTS updated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP;

DROP INDEX IF EXISTS _pg_llm_catalog.pg_llm_vectors_vector_idx;
DROP TABLE IF EXISTS _pg_llm_catalog.pg_llm_vectors;
DROP TABLE IF EXISTS _pg_llm_catalog.pg_llm_queries;

CREATE TABLE _pg_llm_catalog.pg_llm_queries (
  id bigserial PRIMARY KEY,
  question vector(64) NOT NULL,
  nl_sql_pair text NOT NULL,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE _pg_llm_catalog.pg_llm_vectors (
  id BIGSERIAL PRIMARY KEY,
  table_name text NOT NULL,
  column_name text NOT NULL,
  row_id bigint NOT NULL,
  query_vector vector(64) NOT NULL,
  metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX pg_llm_vectors_query_vector_idx
  ON _pg_llm_catalog.pg_llm_vectors
  USING ivfflat (query_vector vector_cosine_ops) WITH (lists = 16);

CREATE TABLE IF NOT EXISTS _pg_llm_catalog.pg_llm_sessions (
  session_id text PRIMARY KEY,
  state jsonb NOT NULL DEFAULT '{}'::jsonb,
  max_messages integer NOT NULL DEFAULT 10,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_active_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS _pg_llm_catalog.pg_llm_session_messages (
  id bigserial PRIMARY KEY,
  session_id text NOT NULL REFERENCES _pg_llm_catalog.pg_llm_sessions(session_id) ON DELETE CASCADE,
  request_id uuid NOT NULL DEFAULT '00000000-0000-0000-0000-000000000000',
  role text NOT NULL,
  content text NOT NULL,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS _pg_llm_catalog.pg_llm_audit_log (
  id bigserial PRIMARY KEY,
  request_id uuid NOT NULL,
  event_type text NOT NULL,
  instance_name text NOT NULL DEFAULT '',
  session_id text NOT NULL DEFAULT '',
  success boolean NOT NULL DEFAULT true,
  confidence_score double precision NOT NULL DEFAULT 0,
  metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS _pg_llm_catalog.pg_llm_trace_log (
  id bigserial PRIMARY KEY,
  request_id uuid NOT NULL,
  stage text NOT NULL,
  details jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS _pg_llm_catalog.pg_llm_reports (
  id bigserial PRIMARY KEY,
  request_id uuid NOT NULL UNIQUE,
  instance_name text NOT NULL,
  sql_text text NOT NULL,
  report jsonb NOT NULL,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS _pg_llm_catalog.pg_llm_knowledge_documents (
  id bigserial PRIMARY KEY,
  source_name text NOT NULL,
  content text NOT NULL,
  metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS _pg_llm_catalog.pg_llm_knowledge_chunks (
  id bigserial PRIMARY KEY,
  document_id bigint NOT NULL REFERENCES _pg_llm_catalog.pg_llm_knowledge_documents(id) ON DELETE CASCADE,
  chunk_index integer NOT NULL,
  content text NOT NULL,
  embedding vector(64) NOT NULL,
  metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS _pg_llm_catalog.pg_llm_feedback (
  id bigserial PRIMARY KEY,
  request_id uuid NOT NULL,
  rating integer NOT NULL,
  feedback text NOT NULL,
  metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION pg_llm_chat_json(
  instance_name text,
  prompt text,
  options jsonb DEFAULT '{}'::jsonb
) RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_chat_json'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_chat_stream(
  instance_name text,
  prompt text,
  options jsonb DEFAULT '{}'::jsonb
) RETURNS TABLE (
  seq_no integer,
  chunk text,
  is_final boolean,
  model_name text,
  confidence_score float8,
  request_id uuid
)
AS 'MODULE_PATHNAME', 'pg_llm_chat_stream'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_parallel_chat_json(
  prompt text,
  model_names text[] DEFAULT '{}'::text[],
  options jsonb DEFAULT '{}'::jsonb
) RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_parallel_chat_json'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_text2sql_json(
  instance_name text,
  prompt text,
  schema_info text DEFAULT NULL,
  use_vector_search boolean DEFAULT true,
  options jsonb DEFAULT '{}'::jsonb
) RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_text2sql_json'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_execute_sql_with_analysis(
  instance_name text,
  sql text,
  options jsonb DEFAULT '{}'::jsonb
) RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_execute_sql_with_analysis'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_generate_report(
  instance_name text,
  sql text,
  options jsonb DEFAULT '{}'::jsonb
) RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_generate_report'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_multi_turn_chat_stream(
  instance_name text,
  session_id text,
  prompt text,
  options jsonb DEFAULT '{}'::jsonb
) RETURNS TABLE (
  seq_no integer,
  chunk text,
  is_final boolean,
  model_name text,
  confidence_score float8,
  request_id uuid
)
AS 'MODULE_PATHNAME', 'pg_llm_multi_turn_chat_stream'
LANGUAGE C;

CREATE FUNCTION pg_llm_get_session(session_id text)
RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_get_session'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_get_session_messages(session_id text)
RETURNS TABLE (
  id bigint,
  request_id uuid,
  role text,
  content text,
  created_at timestamptz
)
AS 'MODULE_PATHNAME', 'pg_llm_get_session_messages'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_update_session_state(session_id text, state jsonb)
RETURNS void
AS 'MODULE_PATHNAME', 'pg_llm_update_session_state'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_delete_session(session_id text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'pg_llm_delete_session'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_add_knowledge(
  source_name text,
  content text,
  metadata jsonb DEFAULT '{}'::jsonb,
  options jsonb DEFAULT '{}'::jsonb
) RETURNS bigint
AS 'MODULE_PATHNAME', 'pg_llm_add_knowledge'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_search_knowledge(
  query text,
  options jsonb DEFAULT '{}'::jsonb
) RETURNS TABLE (
  chunk_id bigint,
  document_id bigint,
  chunk_index integer,
  source_name text,
  content text,
  similarity float4,
  metadata jsonb
)
AS 'MODULE_PATHNAME', 'pg_llm_search_knowledge'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_record_feedback(
  request_id uuid,
  rating integer,
  feedback text,
  metadata jsonb DEFAULT '{}'::jsonb
) RETURNS bigint
AS 'MODULE_PATHNAME', 'pg_llm_record_feedback'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_get_audit_log(options jsonb DEFAULT '{}'::jsonb)
RETURNS TABLE (
  request_id uuid,
  event_type text,
  instance_name text,
  session_id text,
  success boolean,
  confidence_score float8,
  metadata jsonb
)
AS 'MODULE_PATHNAME', 'pg_llm_get_audit_log'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_get_trace(request_id uuid)
RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_get_trace'
LANGUAGE C STRICT VOLATILE;
