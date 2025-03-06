#ifndef SQL_OPTIMIZER_H
#define SQL_OPTIMIZER_H

#include "postgres.h"

// Get SQL optimization suggestions
char* get_sql_optimization(const char *sql);

// Check SQL performance
float analyze_sql_performance(const char *sql);

// Use LLM to rewrite SQL
char* rewrite_sql_with_llm(const char *sql, const char *model_name);

#endif /* SQL_OPTIMIZER_H */ 