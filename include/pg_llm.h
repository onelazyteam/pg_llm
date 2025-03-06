#ifndef PG_LLM_H
#define PG_LLM_H

#include "postgres.h"
#include "fmgr.h"
#include "model_interface.h"
#include "sql_optimizer.h"
#include "report_generator.h"
#include "conversation.h"
#include "chat.h"  // Add general chat header file

// Version information
#define PG_LLM_VERSION "1.0.0"
#define PG_LLM_AUTHOR "Yang Hao"

// Error codes
#define PG_LLM_ERROR                1
#define PG_LLM_CONFIG_ERROR         2
#define PG_LLM_SECURITY_ERROR       3
#define PG_LLM_MODEL_ERROR          4

// Exported functions
extern Datum pg_llm_version(PG_FUNCTION_ARGS);
extern Datum pg_llm_query(PG_FUNCTION_ARGS);
extern Datum pg_llm_optimize(PG_FUNCTION_ARGS);
extern Datum pg_llm_report(PG_FUNCTION_ARGS);
extern Datum pg_llm_conversation(PG_FUNCTION_ARGS);
extern Datum pg_llm_configure(PG_FUNCTION_ARGS);
extern Datum pg_llm_chat(PG_FUNCTION_ARGS);  // Add general chat function

#endif // PG_LLM_H 