# pg_llm 架构设计（v1.1）

## 1. 总览

`pg_llm` 是一个 PostgreSQL 扩展，通过 SQL 原生函数提供大模型能力。扩展将会话、审计、追踪、报告、知识库等数据持久化到 `_pg_llm_catalog`，并在 PostgreSQL backend 内完成模型调用与编排。

设计目标：

- 兼容已有文本接口
- 新增结构化 `jsonb` 与流式 SRF 接口
- 关键状态全部持久化
- 支持置信度路由与本地兜底
- 支持数据库内可观测与安全控制

## 2. 核心组件

### 2.1 SQL 接口层（`src/pg_llm.cpp`）

职责：

- SQL 函数入口与参数解析
- 通过 SPI 读写 `_pg_llm_catalog`
- 输出格式封装（`text`/`jsonb`/SRF）
- 审计与追踪数据落库

### 2.2 模型层（`src/models/*`）

- `LLMInterface`：统一聊天、流式、embedding 接口
- `ModelManager`：模型注册、实例缓存、并行推理
- 按需从 catalog 加载并解密模型密钥
- 内置 mock provider，支持离线确定性测试

### 2.3 Text2SQL 层（`src/text2sql/*`）

- schema 获取（自动/传入）
- 可选向量检索增强
- SQL 生成（`generate_statement`）
- SQL 执行与 `EXPLAIN` 分析

### 2.4 支撑层（`src/utils/*`）

- 核心 GUC 定义
- JSON/UUID 辅助函数
- AES-GCM 加解密
- 敏感字段脱敏
- 基于 PostgreSQL 的原生日志宏（`elog`）

## 3. Catalog 持久化模型

扩展数据统一存于 `_pg_llm_catalog`：

- `pg_llm_models`：模型配置、密钥密文、路由元数据
- `pg_llm_sessions`、`pg_llm_session_messages`：会话状态与消息
- `pg_llm_audit_log`：审计事件
- `pg_llm_trace_log`：中间过程与决策轨迹
- `pg_llm_reports`：报告产物
- `pg_llm_knowledge_documents`、`pg_llm_knowledge_chunks`：知识库
- `pg_llm_feedback`：反馈数据
- `pg_llm_queries`、`pg_llm_vectors`：Text2SQL 向量相关数据

## 4. API 形态

### 4.1 兼容文本接口

- `pg_llm_chat`
- `pg_llm_parallel_chat`
- `pg_llm_text2sql`
- 会话接口（`pg_llm_create_session`、`pg_llm_multi_turn_chat` 等）

这些接口保留，内部复用结构化执行路径。

### 4.2 结构化接口

- `pg_llm_chat_json`、`pg_llm_parallel_chat_json`、`pg_llm_text2sql_json`
- `pg_llm_execute_sql_with_analysis`、`pg_llm_generate_report`
- `pg_llm_get_session`、`pg_llm_get_session_messages`、`pg_llm_update_session_state`、`pg_llm_delete_session`
- `pg_llm_add_knowledge`、`pg_llm_search_knowledge`
- `pg_llm_record_feedback`
- `pg_llm_get_audit_log`、`pg_llm_get_trace`

### 4.3 流式接口

- `pg_llm_chat_stream`
- `pg_llm_multi_turn_chat_stream`

统一返回字段：`seq_no`、`chunk`、`is_final`、`model_name`、`confidence_score`、`request_id`。

## 5. 关键执行流程

### 5.1 聊天 / 多轮聊天

1. 解析并加载模型实例。
2. 按需拼接 RAG 上下文（`options.enable_rag`）。
3. 调用阻塞或流式模型接口。
4. 执行置信度阈值判断与兜底模型切换。
5. 多轮模式下写入会话消息。
6. 记录审计与追踪。

### 5.2 并行聊天路由

1. 并行调用候选模型。
2. 选择最高置信度结果。
3. 计算有效阈值（模型配置/GUC/options）。
4. 低于阈值时走 fallback 模型。
5. 持久化候选分数与决策信息。

### 5.3 Text2SQL

1. 组装 schema 上下文。
2. 可选向量检索与相似样例召回。
3. 生成 SQL。
4. 执行 SQL 并返回结构化结果。
5. 执行 `EXPLAIN` 并返回计划。
6. 记录 trace/audit。

### 5.4 报告生成

1. 执行 SQL 并获取结果与执行计划。
2. 让模型生成叙述性总结。
3. 生成包含建议和 Vega-Lite 规范的 JSON。
4. 报告持久化。

### 5.5 知识库与反馈

- 文档按块切分并生成 embedding 后入库。
- 检索按向量相似度并结合关键词命中提升排序。
- 反馈按 `request_id` 入库，供后续流程使用。

## 6. 安全与可观测

### 6.1 核心 GUC

- `pg_llm.master_key`
- `pg_llm.audit_enabled`
- `pg_llm.trace_enabled`
- `pg_llm.redact_sensitive`
- `pg_llm.audit_sample_rate`
- `pg_llm.default_confidence_threshold`
- `pg_llm.default_local_fallback`

### 6.2 密钥安全

- API Key 和敏感配置先加密再落库。
- 解密仅发生在 backend 内存。
- 审计/追踪输出可按配置自动脱敏。

### 6.3 可观测

- `pg_llm_audit_log`：请求结果与摘要信息。
- `pg_llm_trace_log`：中间步骤与决策细节。
- `request_id`：跨接口/表关联主键。

## 7. 构建与发布

- 扩展版本：`1.1`
- 升级脚本：`pg_llm--1.0--1.1.sql`
- 主编译安装方式：CMake（`contrib/pg_llm/CMakeLists.txt`）
- SQL 回归测试：`test/sql` 与 `test/expected`
