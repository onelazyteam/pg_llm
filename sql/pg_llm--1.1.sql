-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_llm" to load this file. \quit

CREATE SCHEMA _pg_llm_catalog;
GRANT USAGE ON SCHEMA _pg_llm_catalog TO PUBLIC;

CREATE TABLE _pg_llm_catalog.pg_llm_models (
  local_model boolean NOT NULL DEFAULT false,
  model_type text NOT NULL,
  instance_name text PRIMARY KEY,
  api_key text NOT NULL DEFAULT '',
  config text NOT NULL DEFAULT '{}',
  encrypted_api_key text NOT NULL DEFAULT '',
  encrypted_config text NOT NULL DEFAULT '',
  confidence_threshold double precision NOT NULL DEFAULT 0,
  fallback_instance text NOT NULL DEFAULT '',
  is_local_fallback boolean NOT NULL DEFAULT false,
  capabilities jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

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

CREATE TABLE _pg_llm_catalog.pg_llm_sessions (
  session_id text PRIMARY KEY,
  state jsonb NOT NULL DEFAULT '{}'::jsonb,
  max_messages integer NOT NULL DEFAULT 10,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_active_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE _pg_llm_catalog.pg_llm_session_messages (
  id bigserial PRIMARY KEY,
  session_id text NOT NULL REFERENCES _pg_llm_catalog.pg_llm_sessions(session_id) ON DELETE CASCADE,
  request_id uuid NOT NULL,
  role text NOT NULL,
  content text NOT NULL,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX pg_llm_session_messages_session_idx
  ON _pg_llm_catalog.pg_llm_session_messages(session_id, id);

CREATE TABLE _pg_llm_catalog.pg_llm_audit_log (
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

CREATE INDEX pg_llm_audit_request_idx
  ON _pg_llm_catalog.pg_llm_audit_log(request_id, created_at);

CREATE TABLE _pg_llm_catalog.pg_llm_trace_log (
  id bigserial PRIMARY KEY,
  request_id uuid NOT NULL,
  stage text NOT NULL,
  details jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX pg_llm_trace_request_idx
  ON _pg_llm_catalog.pg_llm_trace_log(request_id, id);

CREATE TABLE _pg_llm_catalog.pg_llm_reports (
  id bigserial PRIMARY KEY,
  request_id uuid NOT NULL UNIQUE,
  instance_name text NOT NULL,
  sql_text text NOT NULL,
  report jsonb NOT NULL,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE _pg_llm_catalog.pg_llm_knowledge_documents (
  id bigserial PRIMARY KEY,
  source_name text NOT NULL,
  content text NOT NULL,
  metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE _pg_llm_catalog.pg_llm_knowledge_chunks (
  id bigserial PRIMARY KEY,
  document_id bigint NOT NULL REFERENCES _pg_llm_catalog.pg_llm_knowledge_documents(id) ON DELETE CASCADE,
  chunk_index integer NOT NULL,
  content text NOT NULL,
  embedding vector(64) NOT NULL,
  metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX pg_llm_knowledge_embedding_idx
  ON _pg_llm_catalog.pg_llm_knowledge_chunks
  USING ivfflat (embedding vector_cosine_ops) WITH (lists = 16);

CREATE TABLE _pg_llm_catalog.pg_llm_feedback (
  id bigserial PRIMARY KEY,
  request_id uuid NOT NULL,
  rating integer NOT NULL,
  feedback text NOT NULL,
  metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
  created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION pg_llm_store_vector(
  table_name text,
  column_name text,
  row_id bigint,
  query_vector vector,
  metadata jsonb DEFAULT NULL
) RETURNS bigint
AS 'MODULE_PATHNAME', 'pg_llm_store_vector'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_search_vectors(
  query_vector vector,
  limit_count integer DEFAULT 10,
  similarity_threshold float4 DEFAULT 0.7
) RETURNS TABLE (
  id bigint,
  table_name text,
  column_name text,
  row_id bigint,
  similarity float4,
  metadata jsonb
)
AS 'MODULE_PATHNAME', 'pg_llm_search_vectors'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_get_embedding(
  instance_name text,
  text_var text
) RETURNS vector
AS 'MODULE_PATHNAME', 'pg_llm_get_embedding'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_add_model(
  local_model boolean,
  model_type text,
  instance_name text,
  api_key text,
  config text
) RETURNS boolean
AS 'MODULE_PATHNAME', 'pg_llm_add_model'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_remove_model(instance_name text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'pg_llm_remove_model'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_chat(instance_name text, prompt text)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_chat'
LANGUAGE C STRICT VOLATILE;

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

CREATE FUNCTION pg_llm_parallel_chat(
  prompt text,
  model_names text[] DEFAULT '{}'::text[]
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_parallel_chat'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_parallel_chat_json(
  prompt text,
  model_names text[] DEFAULT '{}'::text[],
  options jsonb DEFAULT '{}'::jsonb
) RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_parallel_chat_json'
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_text2sql(
  instance_name text,
  prompt text,
  schema_info text DEFAULT NULL,
  use_vector_search boolean DEFAULT true
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_text2sql'
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
LANGUAGE C;

CREATE FUNCTION pg_llm_create_session(max_messages integer DEFAULT 10)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_create_session'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_multi_turn_chat(
  instance_name text,
  session_id text,
  prompt text
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_multi_turn_chat'
LANGUAGE C STRICT VOLATILE;

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
LANGUAGE C VOLATILE;

CREATE FUNCTION pg_llm_set_max_messages(
  session_id text,
  max_messages integer
) RETURNS void
AS 'MODULE_PATHNAME', 'pg_llm_set_max_messages'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_llm_get_sessions()
RETURNS TABLE (
  session_id text,
  message_count integer,
  max_messages integer,
  last_active_time timestamptz
)
AS 'MODULE_PATHNAME', 'pg_llm_get_sessions'
LANGUAGE C VOLATILE;

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

CREATE FUNCTION pg_llm_cleanup_sessions(timeout_seconds integer)
RETURNS void
AS 'MODULE_PATHNAME', 'pg_llm_cleanup_sessions'
LANGUAGE C STRICT;

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

GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO PUBLIC;
