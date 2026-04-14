# 日志兼容层说明

## 1. 当前实现

`pg_llm` 现在通过 PostgreSQL 原生日志（`ereport`）输出日志，同时保留历史 `pg_llm_glog_*` 符号和 GUC 名称以保证兼容。

这意味着：

- 日常构建不再要求外部 glog 运行时依赖。
- 现有 `PG_LLM_LOG_INFO/WARNING/ERROR/FATAL` 调用无需改动。
- 旧的 `pg_llm.glog_*` 配置仍可继续使用。

## 2. 背景

早期版本采用直接 glog 集成。当前兼容层方案用于规避与 PostgreSQL 的宏/符号冲突，并保持扩展接口稳定。

## 3. 头文件使用方式

请统一使用：

```cpp
#include "utils/pg_llm_glog.h"
```

不要在扩展源码中直接包含 `glog/logging.h`。

## 4. 可用日志宏

- `PG_LLM_LOG_INFO(...)`
- `PG_LLM_LOG_WARNING(...)`
- `PG_LLM_LOG_ERROR(...)`
- `PG_LLM_LOG_FATAL(...)`

这些宏最终映射到 PostgreSQL 日志级别。

## 5. 兼容 GUC

以下 GUC 作为兼容项保留：

- `pg_llm.glog_log_dir`
- `pg_llm.glog_min_log_level`
- `pg_llm.glog_log_to_stderr`
- `pg_llm.glog_log_to_system_logger`
- `pg_llm.glog_max_log_size`
- `pg_llm.glog_v`

旧配置可以继续保留；新部署建议以 PostgreSQL 日志体系为主。

## 6. CMake 构建说明

项目主构建方式为 CMake（`contrib/pg_llm/CMakeLists.txt`）。

默认依赖：

- PostgreSQL 头文件与 `pg_config`
- OpenSSL
- libcurl
- JsonCpp

默认不需要单独执行 glog 编译步骤。

## 7. 迁移建议

如果历史环境依赖 glog 侧运维策略，建议：

1. 升级初期保留现有 `pg_llm.glog_*` 配置。
2. 逐步将日志采集与轮转策略迁移到 PostgreSQL 日志配置。
3. 升级后核对 PostgreSQL 日志中的级别映射与输出行为。
