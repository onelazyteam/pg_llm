# Logging Compatibility Notes

## 1. Current Behavior

`pg_llm` now logs through PostgreSQL native logging (`ereport`) while keeping the historical `pg_llm_glog_*` symbols and GUC names for backward compatibility.

This means:

- No runtime dependency on external glog is required for normal builds.
- Existing code using `PG_LLM_LOG_INFO/WARNING/ERROR/FATAL` continues to work.
- Existing `pg_llm.glog_*` settings remain accepted as compatibility knobs.

## 2. Why This Exists

Earlier revisions used direct glog integration. The current compatibility layer avoids symbol/macro conflicts with PostgreSQL while preserving extension API stability.

## 3. Recommended Include Pattern

Always use:

```cpp
#include "utils/pg_llm_glog.h"
```

Do not include `glog/logging.h` directly in extension sources.

## 4. Supported Macros

- `PG_LLM_LOG_INFO(...)`
- `PG_LLM_LOG_WARNING(...)`
- `PG_LLM_LOG_ERROR(...)`
- `PG_LLM_LOG_FATAL(...)`

All macros are routed to PostgreSQL log levels.

## 5. Compatibility GUCs

The following GUCs are retained for compatibility:

- `pg_llm.glog_log_dir`
- `pg_llm.glog_min_log_level`
- `pg_llm.glog_log_to_stderr`
- `pg_llm.glog_log_to_system_logger`
- `pg_llm.glog_max_log_size`
- `pg_llm.glog_v`

They are safe to keep in existing configs. New deployments should primarily rely on PostgreSQL logging configuration.

## 6. CMake Build Impact

Primary build path is CMake (`contrib/pg_llm/CMakeLists.txt`).

The extension requires:

- PostgreSQL server headers / `pg_config`
- OpenSSL
- libcurl
- JsonCpp

No dedicated glog build step is required by default.

## 7. Migration Guidance

If your old deployment had glog-specific operational assumptions:

1. Keep existing `pg_llm.glog_*` settings temporarily.
2. Move log routing/retention strategy to PostgreSQL logging policies.
3. Validate severity mapping in PostgreSQL logs after upgrade.
