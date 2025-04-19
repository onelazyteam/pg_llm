# 		    pg_llm: **PostgreSQL LLM Integration Extension**

​	This PostgreSQL extension enables direct integration with various Large Language Models (LLMs). It supports multiple LLM providers and features like session management, parallel inference, and more.

# Features

- [x] Support remote models
- [x] Support local LLM
- [x] Dynamic model management (add/remove models at runtime)
- [x] Large model metadata persistence
- [x] Importing the log library
- [ ] Support for streaming responses
- [ ] Session-based multi-turn conversation support
- [ ] Parallel inference with multiple models
- [ ] Automatically select the model with the highest confidence (select by score), and use the local model as a backup (fall back to the local model when confidence is low)
- [ ] Sensitive information encryption
- [ ] Audit logging
- [ ] Session state management
- [ ] Execute SQL through plug-in functions and let the LLM give optimization suggestions
- [x] Support natural language query, can enter human language through plug-ins to query database data
- [ ] Generate reports, generate data analysis charts, intelligent analysis: built-in data statistical analysis and visualization suggestion generation
- [ ] Full observability, complete record of decision-making process and intermediate results
- [ ] Improve model accuracy, RAG knowledge base, feedback learning



# Installation

See README[https://github.com/onelazyteam/pg_llm/blob/master/README.md]

```shell
git clone https://github.com/onelazyteam/pg_llm

cd pg_llm/thirdparty
sh build_glog.sh

cd pg_llm
mkdir build && cd build
make
make install
```



# Usage

After building,  add pg_llm to shared_preload_libraries and restart PostgreSQL:

```sql
shared_preload_libraries = 'pg_llm'
```

 Then need to enable the extension in PostgreSQL:

```sql
CREATE EXTENSION pg_llm;

-- Verify
SELECT * FROM pg_available_extensions WHERE name = 'pg_llm';
```



## Adding Models

```sql
-- local models
SELECT pg_llm_add_model(
    true,
    'deepseek',
    'deepseek-r1-local',
    '',
    '{
        "model_name": "deepseek-r1:1.5b",
        "api_endpoint": "http://localhost:11434/v1/chat/completions",
        "access_key_id": "",
        "access_key_secret": ""
    }'
);

-- remote models
SELECT pg_llm_add_model(
    false,
    'qianwen',
    'qianwen-chat',
    'api-key',
    '{
        "model_name": "qwen-plus",
        "api_endpoint": "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions",
        "access_key_id": "xxx",
        "access_key_secret": "xxx"
    }'
);
```



## Removing Models

```sql
SELECT pg_llm_remove_model('gpt4-chat');
```



## Single-turn Chat

```sql
SELECT pg_llm_chat('qianwen-chat', 'Who are you?');
```



## Text2SQL

```sql
SELECT pg_llm_text2sql('qianwen-chat', 'Create a user table.', NULL, false);
```



# Query model information

```sql
SELECT * FROM _pg_llm_catalog.pg_llm_models ;
```

