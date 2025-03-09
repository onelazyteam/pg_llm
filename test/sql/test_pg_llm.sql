-- Create extension
CREATE EXTENSION pg_llm;

-- Test adding ChatGPT model
SELECT pg_llm_add_model('chatgpt', 'gpt4', 'test-api-key', '{"model_name": "gpt-4"}');

-- Test adding DeepSeek model
SELECT pg_llm_add_model('deepseek', 'deepseek-chat', 'test-api-key', '{"model_name": "deepseek-chat"}');

-- Test adding Hunyuan model
SELECT pg_llm_add_model('hunyuan', 'hunyuan-chat', 'test-api-key', '{"model_name": "hunyuan"}');

-- Test adding Qianwen model
SELECT pg_llm_add_model('qianwen', 'qianwen-chat', 'test-api-key', '{"model_name": "qwen-turbo"}');

-- Test session creation
SELECT session_id FROM pg_llm_create_session('gpt4') \gset
SELECT :session_id AS created_session_id;

-- Test session chat
SELECT pg_llm_chat_session(:session_id, 'What is PostgreSQL?');
SELECT pg_llm_chat_session(:session_id, 'What are its main features?');

-- Test session history
SELECT * FROM pg_llm_get_session_history(:session_id);

-- Test session list
SELECT * FROM pg_llm_list_sessions();

-- Test single-turn chat
SELECT pg_llm_chat('gpt4', 'Tell me about databases.');

-- Test parallel chat with multiple models
SELECT pg_llm_parallel_chat(
    'What are the advantages of PostgreSQL?',
    ARRAY['gpt4', 'deepseek-chat', 'hunyuan-chat', 'qianwen-chat']
);

-- Test session deletion
SELECT pg_llm_delete_session(:session_id);

-- Test removing models
SELECT pg_llm_remove_model('gpt4');
SELECT pg_llm_remove_model('deepseek-chat');
SELECT pg_llm_remove_model('hunyuan-chat');
SELECT pg_llm_remove_model('qianwen-chat');

-- Drop extension
DROP EXTENSION pg_llm;
