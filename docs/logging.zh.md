# 日志设计

## 概述

`pg_llm` 直接使用 PostgreSQL 原生日志能力，不再依赖外部 glog。

## 头文件

请使用：

```cpp
#include "utils/pg_llm_log.h"
```

可用日志宏：

- `PG_LLM_LOG_INFO(...)`
- `PG_LLM_LOG_WARNING(...)`
- `PG_LLM_LOG_ERROR(...)`
- `PG_LLM_LOG_FATAL(...)`

## 级别映射

- `INFO` -> `elog(LOG, ...)`
- `WARNING` -> `elog(WARNING, ...)`
- `ERROR` -> `elog(WARNING, ...)`
- `FATAL` -> `elog(ERROR, ...)`

其中 `FATAL` 会抛出 PostgreSQL 错误，其他级别仅记录日志。

## 运行建议

- 日志输出、轮转与保留策略统一通过 PostgreSQL 日志配置管理。
- 部署时不再需要任何 glog 编译或运行时依赖。
