# pg_llm API Documentation

## Function Reference

### 1. Natural Language Query

```sql
pg_llm_query(query text, model_name text DEFAULT NULL) RETURNS text
```

Convert natural language to SQL and execute.

Parameters:
- query: Natural language query description
- model_name: Model name to use (optional)

Example:
```sql
SELECT pg_llm_query('Show the top 5 products by sales in the last week');
```

### 2. SQL Optimization

```sql
pg_llm_optimize(sql text) RETURNS text
```

Analyze SQL and provide optimization suggestions.

Parameters:
- sql: SQL statement to optimize

Example:
```sql
SELECT pg_llm_optimize('
  SELECT * FROM orders 
  WHERE created_at > NOW() - INTERVAL ''30 days''
');
```

### 3. Report Generation

```sql
pg_llm_report(description text) RETURNS text
pg_llm_report_typed(description text, report_type text) RETURNS text
```

Generate report based on description.

Parameters:
- description: Report requirement description
- report_type: Report type (trend/comparison/summary/forecast)

Example:
```sql
SELECT pg_llm_report('Generate a month-over-month analysis report of sales by region for the past year');
```

### 4. Multi-turn Conversation

```sql
pg_llm_conversation(message text, conversation_id integer) RETURNS text
```

Support context-aware multi-turn conversations.

Parameters:
- message: Conversation message
- conversation_id: Conversation ID

Example:
```sql
SELECT pg_llm_conversation('Analyze the sales trend for the last three months', 1);
SELECT pg_llm_conversation('How does it compare to the same period last year?', 1);
```

### 5. Configuration Management

```sql
pg_llm_configure(config json) RETURNS boolean
```

Update plugin configuration.

Parameters:
- config: Configuration in JSON format

Example:
```sql
SELECT pg_llm_configure('{
  "models": {
    "chatgpt": {
      "api_key": "your-api-key",
      "enabled": true
    }
  }
}');
```

### 6. General Chat

```sql
pg_llm_chat(message text, model_name text DEFAULT NULL) RETURNS text
```

Have general conversations with large language models, can ask any questions.

Parameters:
- message: User message
- model_name: Model name to use (optional)

Example:
```sql
-- Technical questions
SELECT pg_llm_chat('Explain the principles of MVCC');

-- Knowledge Q&A
SELECT pg_llm_chat('What is quantum computing?');

-- Text generation
SELECT pg_llm_chat('Write a poem about spring');

-- Code related
SELECT pg_llm_chat('How to optimize PostgreSQL query performance?');

-- Specify model
SELECT pg_llm_chat('Tell me a joke', 'chatgpt');
```

## Error Handling

All functions throw exceptions when encountering errors. Error types include:

- `pg_llm_error`: General error
- `pg_llm_config_error`: Configuration error
- `pg_llm_security_error`: Security-related error
- `pg_llm_model_error`: Model invocation error

Example:
```sql
DO $$
BEGIN
  BEGIN
    PERFORM pg_llm_query('DROP DATABASE test');
  EXCEPTION
    WHEN pg_llm_security_error THEN
      RAISE NOTICE 'Operation rejected by security policy';
  END;
END;
$$;
```
