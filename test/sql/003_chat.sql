-- General chat test
\set VERBOSITY terse

-- Test basic chat
SELECT ok(
    pg_llm_chat('Hello') IS NOT NULL,
    'Basic greeting should return result'
);

-- Test technical Q&A
SELECT ok(
    pg_llm_chat('What is a database index?') IS NOT NULL,
    'Technical Q&A should return result'
);

-- Test multilingual
SELECT ok(
    pg_llm_chat('How are you?') IS NOT NULL,
    'English chat should return result'
);

SELECT ok(
    pg_llm_chat('こんにちは') IS NOT NULL,
    'Japanese chat should return result'
);

-- Test long text generation
SELECT ok(
    length(pg_llm_chat('Write an article about the history of databases')) > 500,
    'Long text generation should return sufficient length'
);

-- Test code generation
SELECT ok(
    pg_llm_chat('Write a PostgreSQL stored procedure example') ~* 'CREATE.*PROCEDURE',
    'Code generation should return valid code'
);

-- Test different models
SELECT ok(
    pg_llm_chat('Hello', 'chatgpt') IS NOT NULL,
    'ChatGPT model should work normally'
);

SELECT ok(
    pg_llm_chat('Hello', 'qianwen') IS NOT NULL,
    'Qianwen model should work normally'
); 