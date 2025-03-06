-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_llm" to load this file. \quit

-- Create configuration function
CREATE FUNCTION pg_llm_configure(config jsonb)
RETURNS void
AS 'MODULE_PATHNAME', 'pg_llm_configure'
LANGUAGE C STRICT;

-- Create chat function
CREATE FUNCTION pg_llm_chat(
    message text,
    model text DEFAULT NULL,
    system_prompt text DEFAULT NULL,
    temperature float DEFAULT 0.7,
    max_tokens integer DEFAULT 1000
)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_chat'
LANGUAGE C STRICT;

-- Create natural language query function
CREATE FUNCTION pg_llm_query(query text)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_query'
LANGUAGE C STRICT;

-- Create SQL optimization function
CREATE FUNCTION pg_llm_optimize(sql text)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_optimize'
LANGUAGE C STRICT;

-- Create report generation function
CREATE FUNCTION pg_llm_generate_report(
    query text,
    config jsonb DEFAULT '{
        "type": "mixed",
        "include_chart": true,
        "chart_type": "line",
        "include_table": true,
        "include_summary": true,
        "format": "json"
    }'::jsonb
)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_generate_report'
LANGUAGE C STRICT;

-- Create visualization URL generation function
CREATE FUNCTION pg_llm_get_visualization_url(report_id text)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_get_visualization_url'
LANGUAGE C STRICT;

-- Create conversation function
CREATE FUNCTION pg_llm_conversation(message text, conversation_id integer DEFAULT NULL)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_conversation'
LANGUAGE C;

-- Create debug function
CREATE FUNCTION pg_llm_debug(message text)
RETURNS void
AS 'MODULE_PATHNAME', 'pg_llm_debug'
LANGUAGE C STRICT;

-- Create get configuration function
CREATE FUNCTION pg_llm_get_config()
RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_get_config'
LANGUAGE C STRICT;

-- Create reset configuration function
CREATE FUNCTION pg_llm_reset_config()
RETURNS void
AS 'MODULE_PATHNAME', 'pg_llm_reset_config'
LANGUAGE C STRICT;

-- Create version information function
CREATE FUNCTION pg_llm_version()
RETURNS text
AS 'MODULE_PATHNAME', 'pg_llm_version'
LANGUAGE C STRICT;

-- Create health check function
CREATE FUNCTION pg_llm_health_check()
RETURNS jsonb
AS 'MODULE_PATHNAME', 'pg_llm_health_check'
LANGUAGE C STRICT; 