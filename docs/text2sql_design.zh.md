# Text2SQL 设计文档（v1.1）

## 1. 目标

`pg_llm` 的 Text2SQL 能力将自然语言问题转换为 SQL，并在 PostgreSQL 内执行，支持可选向量增强与结构化可观测输出。

主要接口：

- `pg_llm_text2sql(...) returns text`（兼容旧接口）
- `pg_llm_text2sql_json(...) returns jsonb`（结构化主接口）

## 2. 执行流水线

核心实现入口：`src/pg_llm.cpp` 中的 `build_text2sql_json_internal(...)`。

### 步骤 1：上下文组装

- 从 `ModelManager` 获取模型实例。
- 解析 `options` 到 `Text2SQLConfig`。
- 组装 schema：
  - 使用调用方传入的 `schema_info`，或
  - 调用 `Text2SQL::get_schema_info()` 自动获取。

### 步骤 2：可选向量召回

当 `use_vector_search = true` 时：

- 调用 `Text2SQL::search_vectors(prompt)` 召回相关向量命中。
- 调用 `Text2SQL::get_similar_queries(prompt)` 召回相似 NL-SQL 样例。

向量相关表为 `_pg_llm_catalog.pg_llm_vectors`，字段已统一：

- `query_vector`（向量列）
- `table_name`、`column_name`、`row_id`、`metadata`

### 步骤 3：SQL 生成

- 调用 `Text2SQL::generate_statement(prompt, schema, vector_hits, similar_queries)`。
- 得到规范化 SQL（`generated_sql`）。

### 步骤 4：SQL 执行与分析

- 通过 SPI 执行生成 SQL。
- 返回状态、影响行数、结果行列数据。
- 额外执行 `EXPLAIN <sql>`，并返回计划文本（`execution.explain`）。

### 步骤 5：可观测落库

- 在 `pg_llm_audit_log` 写入审计记录（`event_type = text2sql`）。
- 在 `pg_llm_trace_log` 写入完整追踪信息。
- 使用 `request_id` 关联全链路。

## 3. 输入契约

`pg_llm_text2sql_json(instance_name, prompt, schema_info, use_vector_search, options)`

- `instance_name text`：已注册模型实例名
- `prompt text`：自然语言问题
- `schema_info text default NULL`：可选 schema JSON 覆盖
- `use_vector_search bool default true`：是否开启向量增强
- `options jsonb`：运行时参数

当前实现识别的 `options` 键：

- `enable_cache`
- `cache_ttl_seconds`
- `parallel_processing`
- `max_parallel_threads`
- `similarity_threshold`
- `sample_data_limit`

## 4. 输出契约（`jsonb`）

典型结构：

```json
{
  "request_id": "uuid",
  "instance_name": "model-instance",
  "prompt": "show latest 10 orders",
  "generated_sql": "SELECT ...",
  "execution": {
    "status": "SPI_OK_SELECT",
    "row_count": 10,
    "columns": ["id", "created_at"],
    "rows": [{"id": "1", "created_at": "..."}],
    "explain": ["Seq Scan on ..."]
  },
  "similar_queries": ["..."],
  "vector_hits": [
    {
      "table_name": "orders",
      "column_name": "note",
      "row_id": 123,
      "similarity": 0.91,
      "metadata": {"source": "..."}
    }
  ]
}
```

`pg_llm_text2sql(...)` 仅返回 `generated_sql`，用于兼容历史调用。

## 5. 错误语义

以下场景会抛 PostgreSQL 错误（`ereport(ERROR)`）：

- 模型实例不存在或不可用
- SQL 执行失败
- SPI 调用失败
- `schema_info`/`options` JSON 非法

## 6. 安全说明

- 生成 SQL 以调用者当前数据库权限执行。
- 生产环境应配合角色权限与 schema 权限边界控制。
- 审计与追踪内容遵循 `pg_llm.redact_sensitive` 脱敏策略。

## 7. 构建与发布

- 项目通过 CMake 编译安装（`contrib/pg_llm/CMakeLists.txt`）。
- SQL 版本脚本：`sql/pg_llm--1.1.sql`。
- 升级脚本：`sql/pg_llm--1.0--1.1.sql`。
