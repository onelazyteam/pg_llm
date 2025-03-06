#include "../include/chat.h"
#include "../include/model_interface.h"
#include "postgres.h"
#include "utils/builtins.h"
#include <string.h>

PG_FUNCTION_INFO_V1(pg_llm_chat);

// General chat function
Datum
pg_llm_chat(PG_FUNCTION_ARGS)
{
    text *message = PG_GETARG_TEXT_PP(0);
    text *model_name = PG_NARGS() > 1 ? PG_GETARG_TEXT_PP(1) : NULL;
    
    char *msg_str = text_to_cstring(message);
    char *model_str = model_name ? text_to_cstring(model_name) : NULL;
    
    // System message to guide model behavior
    const char *system_message = 
        "You are a knowledgeable and friendly assistant. Please provide accurate and clear answers to user questions. "
        "If code is involved, provide executable examples. If uncertain, honestly express uncertainty.";
    
    // Call model
    ModelResponse *response = call_model(model_str ? model_str : "chatgpt", 
                                       msg_str, 
                                       system_message);
    
    // Process response
    text *result;
    if (response->successful) {
        result = cstring_to_text(response->response);
    } else {
        result = cstring_to_text("Chat failed: Unable to get model response");
    }
    
    // Clean up resources
    if (model_str) {
        pfree(model_str);
    }
    pfree(msg_str);
    pfree(response);
    
    PG_RETURN_TEXT_P(result);
} 