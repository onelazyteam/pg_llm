-- Create report generation function
CREATE OR REPLACE FUNCTION pg_llm_generate_report(
    query text,
    config jsonb DEFAULT '{
        "type": "mixed",
        "include_chart": true,
        "chart_type": "line",
        "include_table": true,
        "include_summary": true,
        "format": "json"
    }'::jsonb
) RETURNS text
LANGUAGE C
AS 'MODULE_PATHNAME', 'pg_llm_generate_report';

-- Create visualization URL function
CREATE OR REPLACE FUNCTION pg_llm_get_visualization_url(
    report_id text
) RETURNS text
LANGUAGE C
AS 'MODULE_PATHNAME', 'pg_llm_get_visualization_url';

-- Usage example
COMMENT ON FUNCTION pg_llm_generate_report IS 'Generate report, support charts and tables, and return visualization URL';

-- Example: Generate report with chart
SELECT pg_llm_generate_report(
    'SELECT date_trunc(''month'', created_at) as month, 
            COUNT(*) as orders,
            SUM(amount) as total_amount
     FROM orders
     WHERE created_at >= NOW() - INTERVAL ''1 year''
     GROUP BY month
     ORDER BY month',
    '{
        "type": "mixed",
        "title": "Monthly Order Analysis",
        "include_chart": true,
        "chart_type": "line",
        "include_table": true,
        "include_summary": true,
        "format": "json"
    }'::jsonb
);

-- Example: Generate trend analysis report
SELECT pg_llm_generate_report(
    'SELECT date_trunc(''day'', created_at) as date,
            COUNT(*) as orders,
            SUM(amount) as total_amount
     FROM orders
     WHERE created_at >= NOW() - INTERVAL ''30 days''
     GROUP BY date
     ORDER BY date',
    '{
        "type": "trend",
        "title": "30-Day Order Trend",
        "include_chart": true,
        "chart_type": "line",
        "include_table": true,
        "include_summary": true,
        "format": "json"
    }'::jsonb
);

-- Example: Generate comparison analysis report
SELECT pg_llm_generate_report(
    'SELECT category,
            COUNT(*) as orders,
            SUM(amount) as total_amount
     FROM orders
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

-- Example: Get report visualization URL
SELECT pg_llm_get_visualization_url('report_1234567890'); 