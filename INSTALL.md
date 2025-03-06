# pg_llm Installation and Usage Guide

## 1. System Requirements

- PostgreSQL 12.0 or higher
- C compiler (GCC or Clang)
- libcurl development library
- json-c development library
- make tools

## 2. Installation Steps

### 2.1 Install Dependencies

On Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y build-essential libcurl4-openssl-dev libjson-c-dev postgresql-server-dev-all
```

On CentOS/RHEL:
```bash
sudo yum groupinstall "Development Tools"
sudo yum install -y libcurl-devel json-c-devel postgresql-devel
```

On macOS:
```bash
brew install libcurl json-c postgresql
```

### 2.2 Compile and Install Plugin

```bash
cd contrib/pg_llm
make
sudo make install
```

### 2.3 Configure PostgreSQL

1. Modify postgresql.conf:
```bash
sudo vim /etc/postgresql/[version]/main/postgresql.conf
```

2. Add the following configuration:
```ini
shared_preload_libraries = 'pg_llm'
```

3. Restart PostgreSQL service:
```bash
sudo systemctl restart postgresql
```

### 2.4 Create Extension

Connect to PostgreSQL database:
```bash
psql -U postgres -d your_database
```

Create extension:
```sql
CREATE EXTENSION pg_llm;
```

## 3. API Key Application

### 3.1 ChatGPT API Key

1. Visit [OpenAI Website](https://platform.openai.com/)
2. Register/Login
3. Go to API key management page
4. Create new API key
5. Save the generated key

### 3.2 Tencent Qianwen API Key

1. Visit [Tencent Cloud Console](https://console.cloud.tencent.com/)
2. Enable Qianwen API service
3. Create API key
4. Save the generated key

### 3.3 Baidu Wenxin API Key

1. Visit [Baidu Intelligent Cloud](https://cloud.baidu.com/)
2. Enable Wenxin API service
3. Create API key and Secret Key
4. Save the generated keys

### 3.4 ByteDance Doubao API Key

1. Visit [ByteDance Open Platform](https://open.bytedance.com/)
2. Enable Doubao API service
3. Create API key
4. Save the generated key

### 3.5 DeepSeek API Key

1. Visit [DeepSeek Website](https://deepseek.com/)
2. Register/Login
3. Enable API service
4. Create API key
5. Save the generated key

### 3.6 Local Model Configuration

1. Download Model File
   - Visit [Hugging Face](https://huggingface.co/) or other model repositories
   - Download suitable model file (e.g., llama-2-7b-chat.gguf)
   - Save the model file to a secure location

2. Install llama.cpp
```bash
# Clone repository
git clone https://github.com/ggerganov/llama.cpp.git
cd llama.cpp

# Compile
make

# Install
sudo make install
```

3. Configure Local Model
```sql
SELECT pg_llm_configure('{
  "models": {
    "local_model": {
      "model_path": "/path/to/your/model.gguf",
      "enabled": true,
      "max_tokens": 1000,
      "temperature": 0.7,
      "threads": 4,
      "context_size": 2048
    }
  }
}');
```

4. Use Local Model
```sql
-- Query using local model
SELECT pg_llm_query('Show me the top 5 products by sales in the last month', 'local_model');

-- Optimize SQL using local model
SELECT pg_llm_optimize('
  SELECT * FROM orders 
  WHERE created_at > NOW() - INTERVAL ''30 days''
  AND status = ''completed''
', 'local_model');
```

Local Model Advantages:
- No network connection required
- No API key needed
- Higher data security
- Faster response time
- No usage limits

Local Model Limitations:
- Requires significant computational resources
- Model performance may be lower than cloud services
- Requires regular model updates
- Occupies large storage space

## 4. Plugin Configuration

### 4.1 Basic Configuration

```sql
SELECT pg_llm_configure('{
  "models": {
    "chatgpt": {
      "api_key": "your-openai-api-key",
      "api_url": "https://api.openai.com/v1/chat/completions",
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
    "deepseek": {
      "api_key": "your-deepseek-api-key",
      "api_url": "https://your-deepseek-endpoint",
      "enabled": true,
      "timeout_ms": 10000,
      "max_tokens": 1000
    }
  }
}');
```

### 4.2 Advanced Configuration

```sql
SELECT pg_llm_configure('{
  "hybrid_reasoning": {
    "confidence_threshold": 0.6,
    "parallel_models": ["chatgpt", "qianwen"],
    "fallback_strategy": "local_model"
  },
  "security": {
    "audit_enabled": true,
    "encrypt_sensitive": true,
    "banned_keywords": [
      "DROP DATABASE",
      "TRUNCATE",
      "ALTER SYSTEM"
    ]
  },
  "conversation": {
    "max_history": 10,
    "expire_minutes": 60
  }
}');
```

## 5. Features Usage

### 5.1 Natural Language Query

```sql
-- Basic query
SELECT pg_llm_query('Show me the top 5 products by sales in the last month');

-- Query with context
SELECT pg_llm_query('How does this trend compare to the same period last year?');
```

Output example:
```json
{
  "sql": "SELECT product_name, SUM(amount) as total_sales 
          FROM orders 
          WHERE created_at >= NOW() - INTERVAL '1 month'
          GROUP BY product_name 
          ORDER BY total_sales DESC 
          LIMIT 5",
  "result": [
    {"product_name": "Product A", "total_sales": 10000},
    {"product_name": "Product B", "total_sales": 8000},
    ...
  ]
}
```

### 5.2 SQL Optimization

```sql
-- Optimize SQL query
SELECT pg_llm_optimize('
  SELECT * FROM orders 
  WHERE created_at > NOW() - INTERVAL ''30 days''
  AND status = ''completed''
');
```

Output example:
```json
{
  "original_sql": "SELECT * FROM orders WHERE created_at > NOW() - INTERVAL '30 days' AND status = 'completed'",
  "optimized_sql": "SELECT order_id, customer_id, amount, created_at FROM orders WHERE created_at > NOW() - INTERVAL '30 days' AND status = 'completed'",
  "suggestions": [
    "Consider adding index on created_at",
    "Select only required fields",
    "Add LIMIT to restrict result set size"
  ]
}
```

### 5.3 Report Generation

```sql
-- Generate monthly sales report
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
```

Output example:
```json
{
  "type": "report",
  "id": "report_1234567890",
  "title": "Monthly Order Analysis",
  "chart": {
    "type": "line",
    "data": {
      "columns": ["month", "orders", "total_amount"],
      "rows": [
        ["2024-01", 100, 50000],
        ["2024-02", 120, 60000],
        ...
      ]
    }
  },
  "visualization_url": "http://localhost:8080/visualization/report_1234567890",
  "table": {
    "columns": ["month", "orders", "total_amount"],
    "rows": [
      ["2024-01", 100, 50000],
      ["2024-02", 120, 60000],
      ...
    ]
  }
}
```

### 5.4 Chat Function

```sql
-- Basic chat
SELECT pg_llm_chat('Hello, please introduce yourself');

-- Chat with specific model
SELECT pg_llm_chat('Explain what a database index is', 'chatgpt');

-- Chat with system prompt
SELECT pg_llm_chat(
    'Help me optimize this SQL query',
    'qianwen',
    'You are a professional database optimization expert, please explain SQL optimization suggestions in simple terms',
    0.7,
    1000
);

-- Chat using local model
SELECT pg_llm_chat('Analyze the sales data for the last three months', 'local_model');
```

Output example:
```json
{
  "response": "Based on the analysis, the sales data for the last three months shows the following characteristics:\n1. Monthly sales are increasing\n2. Customer count shows steady growth\n3. Average order value has improved",
  "model": "chatgpt",
  "tokens_used": 150,
  "confidence": 0.95
}
```

Chat Function Features:
- Supports multi-turn dialogue
- Customizable system prompts
- Adjustable model parameters
- Multiple model support
- Provides confidence scoring

### 5.5 Multi-turn Conversation

```sql
-- Create new conversation
SELECT pg_llm_conversation('Analyze the sales trend for the last three months', 1);

-- Continue conversation
SELECT pg_llm_conversation('How does this trend compare to the same period last year?', 1);
```

Output example:
```json
{
  "response": "Based on the analysis, the sales trend over the last three months shows an upward trend:\n1. January sales: 500k\n2. February sales: 600k\n3. March sales: 700k\n\nCompared to the same period last year, there is approximately 20% growth.",
  "conversation_id": 1,
  "confidence": 0.95
}
```

## 6. Common Issues

### 6.1 Installation Issues

1. Compilation Errors
   - Check if all dependencies are installed
   - Ensure PostgreSQL development package version matches
   - Check compiler version compatibility

2. Loading Errors
   - Check postgresql.conf configuration
   - Check plugin file permissions
   - Check PostgreSQL logs

### 6.2 Usage Issues

1. API Call Failures
   - Verify API key correctness
   - Check network connectivity
   - Check API service status

2. Performance Issues
   - Adjust timeout settings
   - Optimize query statements
   - Check database indexes

### 6.3 Security Recommendations

1. API Key Security
   - Rotate API keys regularly
   - Use principle of least privilege
   - Encrypt key storage

2. Data Security
   - Enable audit logging
   - Set sensitive word filtering
   - Limit query scope

## 7. Change Log

### v1.0.0 (2024-03-20)
- Initial release
- Support for multiple LLM models
- Basic functionality implementation

### v1.1.0 (Planned)
- Add more model support
- Performance optimization
- Enhanced security

## 8. Contact Information

- Issue Reporting: [GitHub Issues](https://github.com/onelazyteam/pg_llm/issues)
- Email Contact: 18611856983@163.com