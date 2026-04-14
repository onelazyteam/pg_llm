#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "postgres.h"

#ifdef __cplusplus
}
#endif

/*
 * pg_llm logging macros backed by PostgreSQL logging.
 *
 * ERROR is intentionally mapped to WARNING to keep previous non-throwing
 * behavior used by existing call sites. FATAL maps to ERROR and raises.
 */
#define PG_LLM_LOG_INFO(...) elog(LOG, __VA_ARGS__)
#define PG_LLM_LOG_WARNING(...) elog(WARNING, __VA_ARGS__)
#define PG_LLM_LOG_ERROR(...) elog(WARNING, __VA_ARGS__)
#define PG_LLM_LOG_FATAL(...) elog(ERROR, __VA_ARGS__)
