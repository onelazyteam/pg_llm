-- Test report generation and visualization functionality

-- Create test table
CREATE TABLE test_orders (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMP NOT NULL,
    category TEXT NOT NULL,
    amount DECIMAL(10,2) NOT NULL
);

-- Insert test data
INSERT INTO test_orders (created_at, category, amount) VALUES
    ('2023-01-01', 'Electronics', 1000.00),
    ('2023-01-02', 'Clothing', 500.00),
    ('2023-01-03', 'Food', 300.00),
    ('2023-02-01', 'Electronics', 1200.00),
    ('2023-02-02', 'Clothing', 600.00),
    ('2023-02-03', 'Food', 400.00),
    ('2023-03-01', 'Electronics', 1500.00),
    ('2023-03-02', 'Clothing', 700.00),
    ('2023-03-03', 'Food', 500.00);

-- Test 1: Generate monthly trend report
SELECT pg_llm_generate_report(
    'SELECT date_trunc(''month'', created_at) as month,
            COUNT(*) as orders,
            SUM(amount) as total_amount
     FROM test_orders
     GROUP BY month
     ORDER BY month',
    '{
        "type": "trend",
        "title": "Monthly Order Trend",
        "include_chart": true,
        "chart_type": "line",
        "include_table": true,
        "include_summary": true,
        "format": "json"
    }'::jsonb
);

-- Test 2: Generate category comparison report
SELECT pg_llm_generate_report(
    'SELECT category,
            COUNT(*) as orders,
            SUM(amount) as total_amount
     FROM test_orders
     GROUP BY category
     ORDER BY total_amount DESC',
    '{
        "type": "comparison",
        "title": "Category Sales Comparison",
        "include_chart": true,
        "chart_type": "bar",
        "include_table": true,
        "include_summary": true,
        "format": "json"
    }'::jsonb
);

-- Test 3: Generate text-only report
SELECT pg_llm_generate_report(
    'SELECT category,
            COUNT(*) as orders,
            SUM(amount) as total_amount
     FROM test_orders
     GROUP BY category
     ORDER BY total_amount DESC',
    '{
        "type": "text",
        "title": "Category Sales Summary",
        "include_chart": false,
        "include_table": true,
        "include_summary": true,
        "format": "text"
    }'::jsonb
);

-- Test 4: Get visualization URL
SELECT pg_llm_get_visualization_url('report_1234567890');

-- Test 5: Error handling
-- Test empty query
SELECT pg_llm_generate_report('', '{}'::jsonb);

-- Test invalid JSON configuration
SELECT pg_llm_generate_report(
    'SELECT * FROM test_orders',
    'invalid json'::jsonb
);

-- Test invalid report ID
SELECT pg_llm_get_visualization_url('');

-- Clean up test data
DROP TABLE test_orders; 