#ifndef PG_LLM_CHAT_H
#define PG_LLM_CHAT_H

#include "postgres.h"
#include "fmgr.h"

// General chat function
Datum pg_llm_chat(PG_FUNCTION_ARGS);

#endif // PG_LLM_CHAT_H 