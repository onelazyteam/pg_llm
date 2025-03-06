-- Model functionality test
\set VERBOSITY terse

-- Test different models
SELECT ok(
    pg_llm_query('Simple query test', 'chatgpt') IS NOT NULL,
    'ChatGPT model should work normally'
);

SELECT ok(
    pg_llm_query('Simple query test', 'qianwen') IS NOT NULL,
    'Qianwen model should work normally'
);

SELECT ok(
    pg_llm_query('Simple query test', 'wenxin') IS NOT NULL,
    'Wenxin model should work normally'
);

SELECT ok(
    pg_llm_query('Simple query test', 'doubao') IS NOT NULL,
    'Doubao model should work normally'
);

SELECT ok(
    pg_llm_query('Simple query test', 'local_model') IS NOT NULL,
    'Local model should work normally'
);

-- Test model switching
SELECT ok(
    pg_llm_configure('{
        "models": {
            "chatgpt": {"enabled": false},
            "qianwen": {"enabled": true}
        }
    }'),
    'Model switching configuration should succeed'
);

SELECT ok(
    pg_llm_query('Test query') ~* 'qianwen',
    'Should use Qianwen model'
);

-- Test hybrid reasoning
SELECT ok(
    pg_llm_configure('{
        "hybrid_reasoning": {
            "parallel_models": ["chatgpt", "qianwen"],
            "confidence_threshold": 0.8
        }
    }'),
    'Hybrid reasoning configuration should succeed'
);

SELECT ok(
    pg_llm_query('Complex query test') IS NOT NULL,
    'Hybrid reasoning should work normally'
); 