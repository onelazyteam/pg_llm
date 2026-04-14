CREATE EXTENSION IF NOT EXISTS vector;
DROP EXTENSION IF EXISTS pg_llm CASCADE;
CREATE EXTENSION pg_llm VERSION '1.0';
ALTER EXTENSION pg_llm UPDATE TO '1.1';

SELECT extversion = '1.1'
FROM pg_extension
WHERE extname = 'pg_llm';

SELECT to_regclass('_pg_llm_catalog.pg_llm_audit_log') IS NOT NULL;
SELECT to_regclass('_pg_llm_catalog.pg_llm_trace_log') IS NOT NULL;
SELECT to_regprocedure('pg_llm_chat_json(text,text,jsonb)') IS NOT NULL;
SELECT to_regprocedure('pg_llm_get_trace(uuid)') IS NOT NULL;

DROP EXTENSION pg_llm CASCADE;
