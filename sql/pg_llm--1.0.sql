-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_llm" to load this file. \quit

-- Create session management table
CREATE TABLE pg_llm_sessions (
    session_id TEXT PRIMARY KEY,
    model_instance TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    last_used_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    metadata JSONB DEFAULT '{}'::jsonb
);

-- Create session history table
CREATE TABLE pg_llm_session_history (
    id BIGSERIAL PRIMARY KEY,
    session_id TEXT REFERENCES pg_llm_sessions(session_id) ON DELETE CASCADE,
    role TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Create functions
CREATE FUNCTION pg_llm_add_model(
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
    FROM pg_llm_session_history
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
    DELETE FROM pg_llm_sessions WHERE session_id = $1;
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
    FROM pg_llm_sessions s
    LEFT JOIN pg_llm_session_history h ON s.session_id = h.session_id
    GROUP BY s.session_id, s.model_instance, s.created_at, s.last_used_at
    ORDER BY s.last_used_at DESC;
$$ LANGUAGE SQL STRICT;

CREATE FUNCTION pg_llm_parallel_chat(
    prompt text,
    model_names text[]
) RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_parallel_chat'
LANGUAGE C STRICT;

-- Grant usage to public
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO PUBLIC;
GRANT SELECT, INSERT, UPDATE, DELETE ON pg_llm_sessions TO PUBLIC;
GRANT SELECT, INSERT ON pg_llm_session_history TO PUBLIC; 