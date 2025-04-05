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

-- Create session management table
CREATE TABLE _pg_llm_catalog.pg_llm_sessions (
  session_id TEXT PRIMARY KEY,
  model_instance TEXT NOT NULL,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
  last_used_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
  metadata JSONB DEFAULT '{}'::jsonb
);

-- Create session history table
CREATE TABLE _pg_llm_catalog.pg_llm_session_history (
  id BIGSERIAL PRIMARY KEY,
  session_id TEXT REFERENCES _pg_llm_catalog.pg_llm_sessions(session_id) ON DELETE CASCADE,
  role TEXT NOT NULL,
  content TEXT NOT NULL,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
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

-- Create a new chat session
CREATE FUNCTION pg_llm_create_session(
  model_instance text,
  session_id text DEFAULT NULL
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_create_session'
LANGUAGE C STRICT;

-- Get session history
CREATE FUNCTION pg_llm_get_session_history(
  session_id text
) RETURNS TABLE (
  role text,
  content text,
  created_at timestamp with time zone
)
AS $$
   SELECT role, content, created_at
   FROM _pg_llm_catalog.pg_llm_session_history
   WHERE session_id = $1
   ORDER BY id ASC;
$$ LANGUAGE SQL STRICT;

-- Chat within a session
CREATE FUNCTION pg_llm_chat_session(
  session_id text,
  prompt text
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_chat_session'
LANGUAGE C STRICT;

-- Delete a session
CREATE FUNCTION pg_llm_delete_session(
  session_id text
) RETURNS boolean
AS $$
  DELETE FROM _pg_llm_catalog.pg_llm_sessions WHERE session_id = $1;
  SELECT FOUND;
$$ LANGUAGE SQL STRICT;

-- List all active sessions
CREATE FUNCTION pg_llm_list_sessions() RETURNS TABLE (
  session_id text,
  model_instance text,
  created_at timestamp with time zone,
  last_used_at timestamp with time zone,
  message_count bigint
)
AS $$
  SELECT s.session_id, s.model_instance, s.created_at, s.last_used_at,
         COUNT(h.id) as message_count
  FROM _pg_llm_catalog.pg_llm_sessions s
  LEFT JOIN _pg_llm_catalog.pg_llm_session_history h ON s.session_id = h.session_id
  GROUP BY s.session_id, s.model_instance, s.created_at, s.last_used_at
  ORDER BY s.last_used_at DESC;
$$ LANGUAGE SQL STRICT;

CREATE FUNCTION pg_llm_parallel_chat(
  prompt text,
  model_names text[]
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

-- Grant usage to public
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO PUBLIC;
GRANT SELECT, INSERT, UPDATE, DELETE ON _pg_llm_catalog.pg_llm_sessions TO PUBLIC;
GRANT SELECT, INSERT ON _pg_llm_catalog.pg_llm_session_history TO PUBLIC; 
