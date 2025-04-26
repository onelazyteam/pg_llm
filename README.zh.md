# pg_llm - PostgreSQL 大语言模型扩展

## 简介

pg_llm 是一个 PostgreSQL 扩展，它集成了大语言模型(LLM)的能力，为数据库提供自然语言处理功能。主要特性包括：

- 自然语言到 SQL 的转换
- 向量相似度搜索
- 智能对话系统
- 多模型支持

## 功能特性

1. **Text2SQL**
   - 自然语言查询转换为 SQL
   - 向量检索增强
   - 智能提示词优化

2. **向量搜索**
   - 高效的相似度搜索
   - 可配置的相似度阈值
   - 并行处理支持

3. **对话系统**
   - 多轮对话支持

4. **性能优化**
   - 多级缓存机制
   - 并行处理
   - 资源管理

## 安装要求

- PostgreSQL 12+
- pgvector 扩展
- C++17 编译器
- CMake 3.12+

## 安装步骤

1. 克隆仓库：
```bash
git clone https://github.com/your-repo/pg_llm.git
cd pg_llm
```

2. 编译安装：
```bash
mkdir build && cd build
cmake ..
make
make install
```

3. 在 PostgreSQL 中启用扩展：
```sql
CREATE EXTENSION vector;
CREATE EXTENSION pg_llm;
```

## 配置

1. 添加模型：
```sql
SELECT pg_llm_add_model('chatgpt', 'my_model', 'your-api-key', '{"temperature": 0.7}');
```

2. 基本使用：
```sql
-- Text2SQL
SELECT pg_llm_text2sql('my_model', '查询所有用户信息');

-- 向量搜索
SELECT pg_llm_search_vectors(query_vector, 10, 0.7);

-- 对话
SELECT pg_llm_chat('my_model', '你好，请介绍一下这个数据库');

-- 多轮对话
-- 1. 创建会话
SELECT pg_llm_create_session();

-- 2. 使用会话进行对话
SELECT pg_llm_multi_turn_chat('my_model', 'session_id', '你好，请介绍一下这个数据库');
SELECT pg_llm_multi_turn_chat('my_model', 'session_id', '那它有什么特点呢？');
SELECT pg_llm_multi_turn_chat('my_model', 'session_id', '如何安装和配置？');

-- 3. 清理过期会话（可选）
SELECT pg_llm_cleanup_sessions(3600);

-- 4. 查询所有session信息
SELECT pg_llm_get_sessions();
```

## 性能调优

1. 缓存配置：
```sql
SELECT pg_llm_text2sql('my_model', '查询', NULL, true,
  '{"enable_cache": true, "cache_ttl_seconds": 7200}');
```

2. 并行处理：
```sql
SELECT pg_llm_text2sql('my_model', '查询', NULL, true,
  '{"parallel_processing": true, "max_parallel_threads": 4}');
```

## 安全建议

1. API 密钥管理
2. 访问权限控制
3. 数据加密
4. 审计日志

## 贡献指南

请参考 [CONTRIBUTING.zh.md](CONTRIBUTING.zh.md)

## 许可证

MIT License

## 联系方式

- 问题报告：GitHub Issues
- 讨论：GitHub Discussions
- 邮件：18611856983@163.com