#include "../include/sql_optimizer.h"
#include "../include/model_interface.h"
#include <string.h>

// Get SQL optimization suggestions
char* get_sql_optimization(const char *sql)
{
    if (!sql) {
        return pstrdup("Invalid SQL");
    }
    
    // System instructions to guide model for SQL optimization
    const char *system_message = "You are a PostgreSQL expert. Analyze the following SQL statement and provide optimization suggestions, "
                                "including index suggestions, query rewriting, table design improvements, etc. Format your answer "
                                "and explain why these optimizations are effective.";
    
    // Construct prompt
    char *prompt = palloc(strlen(sql) + 100);
    sprintf(prompt, "Please provide optimization suggestions for the following SQL:\n\n%s", sql);
    
    // Call LLM to get optimization suggestions
    ModelResponse *response = call_model("chatgpt", prompt, system_message);
    
    char *result;
    if (response->successful) {
        result = pstrdup(response->response);
    } else {
        result = pstrdup("Unable to get SQL optimization suggestions");
    }
    
    // Clean up resources
    pfree(prompt);
    pfree(response);
    
    return result;
}

// Check SQL performance
float analyze_sql_performance(const char *sql)
{
    // This function should integrate with PostgreSQL execution plan analysis
    // Simplified here to return a random score
    return (float)rand() / RAND_MAX;
}

// Use LLM to rewrite SQL
char* rewrite_sql_with_llm(const char *sql, const char *model_name)
{
    if (!sql) {
        return pstrdup("Invalid SQL");
    }
    
    const char *system_message = "You are a PostgreSQL expert. Please rewrite the following SQL to be more efficient, "
                                "while maintaining exactly the same functionality and result set. Only return the rewritten SQL, "
                                "no explanation needed.";
    
    char *prompt = palloc(strlen(sql) + 100);
    sprintf(prompt, "Please rewrite the following SQL to be more efficient:\n\n%s", sql);
    
    ModelResponse *response = call_model(model_name ? model_name : "chatgpt", prompt, system_message);
    
    char *result;
    if (response->successful) {
        result = pstrdup(response->response);
    } else {
        result = pstrdup("Unable to rewrite SQL");
    }
    
    pfree(prompt);
    pfree(response);
    
    return result;
} 