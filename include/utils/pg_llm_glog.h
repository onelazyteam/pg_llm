/*
 * pg_llm_glog.h
 *
 * Google glog integration for pg_llm.
 *
 * This header provides the necessary definitions and macros for
 * integrating Google's glog library with pg_llm. It ensures proper
 * initializations, correct macro definitions to avoid conflicts,
 * and provides convenient logging wrappers.
 */

#pragma once

/* Include PostgreSQL headers */
#ifdef __cplusplus
extern "C" {
#endif

#include "postgres.h"

#ifdef __cplusplus
}
#endif

/* GUC variables for glog configuration */
extern char* pg_llm_glog_log_dir;
extern char* pg_llm_glog_min_log_level;
extern bool pg_llm_glog_log_to_stderr;
extern bool pg_llm_glog_log_to_system_logger;
extern int pg_llm_glog_max_log_size;
extern int pg_llm_glog_v;

/* Function declarations */
#ifdef __cplusplus
extern "C" {
#endif

/* Initialize glog-related GUC variables */
extern void pg_llm_glog_init_guc(void);

/* Initialize glog */
extern void pg_llm_glog_init(void);

/* Shutdown glog */
extern void pg_llm_glog_shutdown(void);

#ifdef __cplusplus
}
#endif

/*
 * C++ only logging functions
 * We avoid including glog headers here to prevent conflicts
 */
#ifdef __cplusplus

#include <cstdarg>
#include <sstream>

/* Simple wrapper functions for logging */
void pg_llm_log_info(const char* file, int line, const char* format, ...);
void pg_llm_log_warning(const char* file, int line, const char* format, ...);
void pg_llm_log_error(const char* file, int line, const char* format, ...);
void pg_llm_log_fatal(const char* file, int line, const char* format, ...);

/* Macros for easier logging */
#define PG_LLM_LOG_INFO(...) pg_llm_log_info(__FILE__, __LINE__, __VA_ARGS__)
#define PG_LLM_LOG_WARNING(...) pg_llm_log_warning(__FILE__, __LINE__, __VA_ARGS__)
#define PG_LLM_LOG_ERROR(...) pg_llm_log_error(__FILE__, __LINE__, __VA_ARGS__)
#define PG_LLM_LOG_FATAL(...) pg_llm_log_fatal(__FILE__, __LINE__, __VA_ARGS__)

#endif /* __cplusplus */
