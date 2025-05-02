-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_llm" to load this file. \quit

CREATE SCHEMA _pg_llm_catalog;
GRANT USAGE ON SCHEMA _pg_llm_catalog TO PUBLIC;

-- Create models catalog table
CREATE TABLE _pg_llm_catalog.pg_llm_models (
  local_model boolean,
  model_type TEXT,
  instance_name TEXT,
  api_key TEXT,
  config TEXT
);

-- Create vector storage table using pgvector
CREATE TABLE _pg_llm_catalog.pg_llm_vectors (
  id BIGSERIAL PRIMARY KEY,
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  row_id BIGINT NOT NULL,
  vector vector(1536) NOT NULL,  -- 使用pgvector的vector类型
  metadata JSONB DEFAULT '{}'::jsonb,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Create index for vector similarity search using pgvector
CREATE INDEX pg_llm_vectors_vector_idx ON _pg_llm_catalog.pg_llm_vectors 
USING ivfflat (vector vector_cosine_ops) WITH (lists = 100);

-- Function to store vectors
CREATE FUNCTION pg_llm_store_vector(
  table_name text,
  column_name text,
  row_id bigint,
  vector vector,  -- 使用pgvector的vector类型
  metadata jsonb DEFAULT NULL
) RETURNS bigint
AS 'MODULE_PATHNAME', 'pg_llm_store_vector'
LANGUAGE C STRICT;

-- Function to search similar vectors
CREATE FUNCTION pg_llm_search_vectors(
  query_vector vector,  -- 使用pgvector的vector类型
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
LANGUAGE C STRICT;

-- Function to get text embeddings
CREATE FUNCTION pg_llm_get_embedding(
  instance_name text,
  text text
) RETURNS vector  -- 使用pgvector的vector类型
AS 'MODULE_PATHNAME', 'pg_llm_get_embedding'
LANGUAGE C STRICT;

-- Create functions
CREATE FUNCTION pg_llm_add_model(
  local_model boolean,
  model_type text,
  instance_name text,
  api_key text,
  config text
) RETURNS boolean
AS 'MODULE_PATHNAME', 'pg_llm_add_model'
LANGUAGE C STRICT;

CREATE FUNCTION pg_llm_remove_model(
  instance_name text
) RETURNS boolean
AS 'MODULE_PATHNAME', 'pg_llm_remove_model'
LANGUAGE C STRICT;

CREATE FUNCTION pg_llm_chat(
  instance_name text,
  prompt text
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_chat'
LANGUAGE C STRICT;

CREATE FUNCTION pg_llm_parallel_chat(
  prompt text,
  model_names text[] DEFAULT '{}'::text[]
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_parallel_chat'
LANGUAGE C STRICT;

-- Enhanced text2sql function with vector search support
CREATE OR REPLACE FUNCTION pg_llm_text2sql(
  instance_name text,
  prompt text,
  schema_info text DEFAULT NULL,
  use_vector_search boolean DEFAULT true
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_text2sql'
LANGUAGE C VOLATILE;

-- Create a new chat session
CREATE OR REPLACE FUNCTION pg_llm_create_session(max_messages integer DEFAULT 10)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_create_session'
LANGUAGE C STRICT;

-- Multi-turn chat with a specific model
CREATE OR REPLACE FUNCTION pg_llm_multi_turn_chat(instance_name text, session_id text, prompt text)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_multi_turn_chat'
LANGUAGE C STRICT;

-- Set maximum number of messages for a session
CREATE OR REPLACE FUNCTION pg_llm_set_max_messages(session_id text, max_messages integer)
RETURNS void
AS 'MODULE_PATHNAME', 'pg_llm_set_max_messages'
LANGUAGE C STRICT;

-- Get all sessions information
CREATE OR REPLACE FUNCTION pg_llm_get_sessions()
RETURNS TABLE (
    session_id text,
    message_count integer,
    max_messages integer,
    last_active_time timestamptz
)
AS 'MODULE_PATHNAME', 'pg_llm_get_sessions'
LANGUAGE C STRICT;

-- Clean up expired sessions
CREATE OR REPLACE FUNCTION pg_llm_cleanup_sessions(timeout_seconds integer)
RETURNS void
AS 'MODULE_PATHNAME', 'pg_llm_cleanup_sessions'
LANGUAGE C STRICT;

-- Grant usage to public
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO PUBLIC;
