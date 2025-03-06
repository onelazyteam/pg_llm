# pg_llm - PostgreSQL Large Language Model Integration Plugin

pg_llm is a PostgreSQL plugin that enables direct use of various large language models in the database, supporting natural language queries, SQL optimization, report generation, and more.

## Features

- Support for multiple large language models:
  - ChatGPT
  - Tencent Qianwen
  - Baidu Wenxin
  - ByteDance Doubao
  - DeepSeek
  - Local Model
- Hybrid reasoning strategy
- Automatic model selection
- Audit and security control
- SQL optimization suggestions
- Natural language report generation
- Multi-turn dialogue support
- Data visualization support:
  - Automatic chart generation
  - Multiple chart types support
  - Visualization URL generation
  - Table display

## Configuration

1. Add to postgresql.conf:
```
shared_preload_libraries = 'pg_llm'
```

2. Create extension:
```sql
CREATE EXTENSION pg_llm;
```

3. Configure model API keys:
```sql
SELECT pg_llm_configure('{
  "models": {
    "chatgpt": {
      "api_key": "your-api-key",
      "enabled": true
    },
    "qianwen": {
      "api_key": "your-api-key",
      "enabled": true
    },
    "wenxin": {
      "api_key": "your-api-key",
      "secret_key": "your-secret-key",
      "enabled": true
    },
    "doubao": {
      "api_key": "your-api-key",
      "enabled": true
    },
    "local_model": {
      "model_path": "/path/to/model.bin",
      "enabled": true
    }
  },
  "hybrid_reasoning": {
    "confidence_threshold": 0.6,
    "parallel_models": ["chatgpt", "qianwen"],
    "fallback_strategy": "local_model"
  }
}');
```

## Usage Examples

1. Natural language query:
```sql
SELECT pg_llm_query('Show me the top 5 products by sales in the last month');
```

2. SQL optimization:
```sql
SELECT pg_llm_optimize('
  SELECT * FROM orders 
  WHERE created_at > NOW() - INTERVAL ''30 days''
  AND status = ''completed''
');
```

3. Generate report and visualization:
```sql
-- Generate monthly order analysis report
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

-- Generate trend analysis report
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

-- Generate comparison analysis report
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
```

4. Multi-turn conversation:
```sql
-- Create conversation
SELECT pg_llm_conversation('Analyze the sales trend for the last three months', 1);
-- Continue conversation
SELECT pg_llm_conversation('How does this trend compare to the same period last year?', 1);
```

## Report Configuration Options

The report generation function supports the following configuration options:

```json
{
    "type": "mixed",           // Report type: text/chart/table/mixed/trend/comparison/summary/forecast
    "title": "Report Title",   // Report title
    "include_chart": true,     // Whether to include chart
    "chart_type": "line",      // Chart type: line/bar/pie/scatter/heatmap
    "include_table": true,     // Whether to include table
    "include_summary": true,   // Whether to include summary
    "format": "json"          // Output format: json/text
}
```

## Visualization Features

1. Automatically generate visualization URL:
```sql
SELECT pg_llm_get_visualization_url('report_1234567890');
```

2. Return data format:
```json
{
    "type": "report",
    "id": "report_1234567890",
    "title": "Report Title",
    "chart": {
        "type": "line",
        "data": {
            "columns": ["Column1", "Column2", "Column3"],
            "rows": [
                ["Value1", "Value2", "Value3"],
                ["Value4", "Value5", "Value6"]
            ]
        }
    },
    "visualization_url": "http://localhost:8080/visualization/report_1234567890",
    "table": {
        "columns": ["Column1", "Column2", "Column3"],
        "rows": [
            ["Value1", "Value2", "Value3"],
            ["Value4", "Value5", "Value6"]
        ]
    }
}
```

## Security

- All queries undergo security checks
- Support for sensitive information encryption
- Complete audit logging
- Configurable access control

## License

MIT License

## Contributing

Issues and Pull Requests are welcome!

## Configuration File Example

```json:pg_llm/config/pg_llm.json.example
{
  "models": {
    "chatgpt": {
      "api_key": "your-openai-api-key",
      "api_url": "https://api.openai.com/v1/chat/completions",
      "model": "gpt-3.5-turbo",
      "enabled": true,
      "timeout_ms": 10000,
      "max_tokens": 1000
    },
    "qianwen": {
      "api_key": "your-qianwen-api-key",
      "api_url": "https://your-qianwen-endpoint",
      "enabled": true,
      "timeout_ms": 10000,
      "max_tokens": 1000
    },
    "wenxin": {
      "api_key": "your-wenxin-api-key",
      "secret_key": "your-wenxin-secret-key",
      "enabled": true,
      "timeout_ms": 10000,
      "max_tokens": 1000
    },
    "doubao": {
      "api_key": "your-doubao-api-key",
      "api_url": "https://your-doubao-endpoint",
      "enabled": true,
      "timeout_ms": 10000,
      "max_tokens": 1000
    },
    "local_model": {
      "model_path": "/path/to/model.bin",
      "enabled": true,
      "max_tokens": 1000,
      "temperature": 0.7,
      "threads": 4,
      "context_size": 2048
    }
  },
  "hybrid_reasoning": {
    "confidence_threshold": 0.6,
    "parallel_models": ["chatgpt", "qianwen"],
    "fallback_strategy": "local_model",
    "max_parallel_calls": 3
  },
  "security": {
    "audit_enabled": true,
    "encrypt_sensitive": true,
    "banned_keywords": [
      "DROP DATABASE",
      "TRUNCATE",
      "ALTER SYSTEM"
    ],
    "sensitive_patterns": [
      "\\d{16}",
      "\\d{3}-\\d{2}-\\d{4}"
    ]
  },
  "conversation": {
    "max_history": 10,
    "expire_minutes": 60,
    "cache_size": 1000
  }
}
```

## Test

The plugin includes a complete test suite, including:

1. Basic functionality test (test/sql/001_basic.sql)
2. Model interface test (test/sql/002_models.sql)
3. Conversation function test (test/sql/003_chat.sql)
4. Report and visualization test (test/sql/test_report_visualization.sql)

Run test:
```bash
make installcheck
```

Test results will be saved in the `test/expected` directory.