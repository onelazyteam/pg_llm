CREATE EXTENSION IF NOT EXISTS vector;
CREATE EXTENSION pg_llm;

SET pg_llm.master_key = 'unit-test-master-key';
SET pg_llm.default_local_fallback = 'mock_local';
SET pg_llm.default_confidence_threshold = 0.50;

SELECT pg_llm_add_model(
  false,
  'mock',
  'mock_primary',
  'topsecret',
  $json$
  {
    "provider": "mock",
    "model_name": "mock-primary",
    "mock_response": "primary reply",
    "mock_confidence": 0.20,
    "fallback_instance": "mock_local",
    "confidence_threshold": 0.50
  }
  $json$
);

SELECT pg_llm_add_model(
  true,
  'mock',
  'mock_local',
  '',
  $json$
  {
    "provider": "mock",
    "model_name": "mock-local",
    "mock_response": "local fallback reply",
    "mock_confidence": 0.95,
    "is_local_fallback": true,
    "mock_chunks": ["local ", "fallback ", "reply"]
  }
  $json$
);

SELECT pg_llm_add_model(
  false,
  'mock',
  'mock_parallel',
  '',
  $json$
  {
    "provider": "mock",
    "model_name": "mock-parallel",
    "mock_response": "parallel winner",
    "mock_confidence": 0.99
  }
  $json$
);

SELECT pg_llm_add_model(
  false,
  'mock',
  'mock_sql',
  '',
  $json$
  {
    "provider": "mock",
    "model_name": "mock-sql",
    "mock_response": "SELECT 42 AS answer;",
    "mock_confidence": 0.95
  }
  $json$
);

SELECT
  encrypted_api_key <> ''
  AND api_key <> 'topsecret'
  AND encrypted_config <> ''
FROM _pg_llm_catalog.pg_llm_models
WHERE instance_name = 'mock_primary';

SELECT pg_llm_chat('mock_primary', 'hello');

SELECT
  (pg_llm_chat_json('mock_primary', 'hello', '{}'::jsonb)->>'response') = 'local fallback reply',
  (pg_llm_chat_json('mock_primary', 'hello', '{}'::jsonb)->>'fallback_used')::boolean;

SELECT
  count(*) > 1,
  bool_or(is_final)
FROM pg_llm_chat_stream('mock_local', 'hello stream', '{}'::jsonb);

SELECT
  pg_llm_parallel_chat(
    'parallel test',
    ARRAY['mock_primary', 'mock_parallel']
  ) = 'parallel winner';

SELECT
  (pg_llm_parallel_chat_json(
    'parallel test',
    ARRAY['mock_primary', 'mock_parallel'],
    '{}'::jsonb
  )->>'response') = 'parallel winner';

SELECT pg_llm_create_session(4) AS session_id \gset
SELECT length(:'session_id') = 36;
SELECT pg_llm_multi_turn_chat('mock_primary', :'session_id', 'first question') = 'local fallback reply';
SELECT pg_llm_update_session_state(:'session_id', '{"topic":"demo"}'::jsonb);
SELECT (pg_llm_get_session(:'session_id')->'state'->>'topic') = 'demo';
SELECT count(*) = 2 FROM pg_llm_get_session_messages(:'session_id');
SELECT
  count(*) > 0,
  bool_or(is_final)
FROM pg_llm_multi_turn_chat_stream('mock_local', :'session_id', 'stream turn', '{}'::jsonb);
SELECT pg_llm_delete_session(:'session_id');

CREATE TABLE pg_llm_demo (label text, value integer);
INSERT INTO pg_llm_demo VALUES ('a', 1), ('b', 2);

SELECT
  (pg_llm_text2sql_json(
    'mock_sql',
    'return answer',
    NULL,
    false,
    '{}'::jsonb
  )->'execution'->'rows'->0->>'answer') = '42';

SELECT
  (pg_llm_execute_sql_with_analysis(
    'mock_sql',
    'SELECT 7 AS x',
    '{}'::jsonb
  )->'rows'->0->>'x') = '7';

SELECT
  (pg_llm_generate_report(
    'mock_local',
    'SELECT ''a'' AS label, 2 AS value',
    '{}'::jsonb
  )->'vega_lite'->>'mark') = 'bar';

SELECT pg_llm_add_knowledge(
  'kb-note',
  'PostgreSQL supports MVCC and extensibility. Vector search can improve retrieval.',
  '{"domain":"database"}'::jsonb,
  '{"chunk_size":32}'::jsonb
) > 0;

SELECT count(*) > 0 FROM pg_llm_search_knowledge('MVCC', '{"limit":3}'::jsonb);

SELECT (pg_llm_chat_json('mock_primary', 'feedback target', '{}'::jsonb)->>'request_id') AS request_id \gset
SELECT length(:'request_id') = 36;
SELECT pg_llm_record_feedback(:'request_id'::uuid, 5, 'useful', '{"tag":"positive"}'::jsonb) > 0;

SELECT count(*) > 0 FROM pg_llm_get_audit_log('{"limit":100}'::jsonb);
SELECT jsonb_array_length((pg_llm_get_trace((SELECT request_id FROM pg_llm_get_audit_log('{"limit":1}'::jsonb) LIMIT 1))->'events')) >= 0;

DROP TABLE pg_llm_demo;
DROP EXTENSION pg_llm CASCADE;
