-- Basic functionality test
\set VERBOSITY terse

-- Create test table
CREATE TABLE test_orders (
    id SERIAL PRIMARY KEY,
    product_name TEXT,
    amount DECIMAL(10,2),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    status TEXT
);

-- Insert test data
INSERT INTO test_orders (product_name, amount, created_at, status) VALUES
('Product A', 100.00, NOW() - INTERVAL '1 day', 'completed'),
('Product B', 200.00, NOW() - INTERVAL '2 days', 'completed'),
('Product C', 300.00, NOW() - INTERVAL '3 days', 'pending'),
('Product D', 400.00, NOW() - INTERVAL '4 days', 'completed'),
('Product E', 500.00, NOW() - INTERVAL '5 days', 'completed');

-- Test natural language query
SELECT ok(
    pg_llm_query('Calculate the total amount of completed orders in the last 3 days') IS NOT NULL,
    'Natural language query should return result'
);

-- Test SQL optimization
SELECT ok(
    pg_llm_optimize('SELECT * FROM test_orders WHERE status = ''completed''') IS NOT NULL,
    'SQL optimization should return suggestions'
);

-- Test report generation
SELECT ok(
    pg_llm_report('Generate daily order amount statistics report') IS NOT NULL,
    'Report generation should return result'
);

-- Test multi-turn conversation
SELECT ok(
    pg_llm_conversation('Analyze order trends', 1) IS NOT NULL,
    'Conversation initialization should succeed'
);

SELECT ok(
    pg_llm_conversation('How does it compare to last week?', 1) IS NOT NULL,
    'Conversation continuation should succeed'
);

-- Test security check
SELECT throws_ok(
    'SELECT pg_llm_query(''DROP DATABASE test'')',
    'Forbidden operation',
    'Should reject dangerous operations'
);

-- Test configuration
SELECT ok(
    pg_llm_configure('{
        "models": {
            "chatgpt": {
                "enabled": false
            }
        }
    }'),
    'Configuration update should succeed'
);

-- Clean up test data
DROP TABLE test_orders; 