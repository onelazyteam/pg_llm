# Logging Design

## Overview

`pg_llm` uses PostgreSQL native logging directly. There is no external glog dependency.

## Header

Use:

```cpp
#include "utils/pg_llm_log.h"
```

The header provides macros:

- `PG_LLM_LOG_INFO(...)`
- `PG_LLM_LOG_WARNING(...)`
- `PG_LLM_LOG_ERROR(...)`
- `PG_LLM_LOG_FATAL(...)`

## Level Mapping

- `INFO` -> `elog(LOG, ...)`
- `WARNING` -> `elog(WARNING, ...)`
- `ERROR` -> `elog(WARNING, ...)`
- `FATAL` -> `elog(ERROR, ...)`

`FATAL` raises PostgreSQL error. Other levels do not.

## Operational Notes

- Configure log destinations and rotation through PostgreSQL logging settings.
- Existing deployments no longer need any glog build step or runtime library.
