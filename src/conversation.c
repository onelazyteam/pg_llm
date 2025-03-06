#include "../include/conversation.h"
#include "postgres.h"
#include "executor/spi.h"
#include "../include/model_interface.h"
#include <string.h>

// Conversation cache
typedef struct {
    int id;
    char **messages;
    char **roles;
    int message_count;
    int capacity;
} ConversationCache;

static ConversationCache **conversation_caches = NULL;
static int cache_count = 0;
static int cache_capacity = 0;

// Initialize conversation context management
void initialize_conversation_context(void)
{
    cache_capacity = 10;
    conversation_caches = palloc(sizeof(ConversationCache*) * cache_capacity);
    cache_count = 0;
}

// Clean up conversation context management
void cleanup_conversation_context(void)
{
    for (int i = 0; i < cache_count; i++) {
        ConversationCache *cache = conversation_caches[i];
        
        for (int j = 0; j < cache->message_count; j++) {
            pfree(cache->messages[j]);
            pfree(cache->roles[j]);
        }
        
        pfree(cache->messages);
        pfree(cache->roles);
        pfree(cache);
    }
    
    pfree(conversation_caches);
}

// Handle conversation message
char* handle_conversation(const char *message, int conversation_id)
{
    if (!message) {
        return pstrdup("Message cannot be empty");
    }
    
    // Find or create conversation cache
    ConversationCache *cache = NULL;
    for (int i = 0; i < cache_count; i++) {
        if (conversation_caches[i]->id == conversation_id) {
            cache = conversation_caches[i];
            break;
        }
    }
    
    if (!cache) {
        // Create new cache
        if (cache_count >= cache_capacity) {
            // Extend cache array
            cache_capacity *= 2;
            conversation_caches = repalloc(conversation_caches, 
                                         sizeof(ConversationCache*) * cache_capacity);
        }
        
        cache = palloc(sizeof(ConversationCache));
        cache->id = conversation_id;
        cache->capacity = 10;
        cache->messages = palloc(sizeof(char*) * cache->capacity);
        cache->roles = palloc(sizeof(char*) * cache->capacity);
        cache->message_count = 0;
        
        conversation_caches[cache_count++] = cache;
    }
    
    // Add user message to cache
    if (cache->message_count >= cache->capacity) {
        // Extend message array
        cache->capacity *= 2;
        cache->messages = repalloc(cache->messages, sizeof(char*) * cache->capacity);
        cache->roles = repalloc(cache->roles, sizeof(char*) * cache->capacity);
    }
    
    cache->roles[cache->message_count] = pstrdup("user");
    cache->messages[cache->message_count] = pstrdup(message);
    cache->message_count++;
    
    // Store in database
    SPI_connect();
    
    // Check if session exists
    char conversation_sql[100];
    sprintf(conversation_sql, "SELECT 1 FROM pg_llm_conversations WHERE id = %d", conversation_id);
    
    int ret = SPI_execute(conversation_sql, true, 0);
    
    if (ret != SPI_OK_SELECT || SPI_processed == 0) {
        // Create new session
        sprintf(conversation_sql, "INSERT INTO pg_llm_conversations (id) VALUES (%d)", conversation_id);
        SPI_execute(conversation_sql, false, 0);
    } else {
        // Update last update time
        sprintf(conversation_sql, "UPDATE pg_llm_conversations SET last_updated = CURRENT_TIMESTAMP WHERE id = %d", 
               conversation_id);
        SPI_execute(conversation_sql, false, 0);
    }
    
    // Insert message
    char insert_sql[1000];
    sprintf(insert_sql, "INSERT INTO pg_llm_messages (conversation_id, role, content) VALUES (%d, 'user', $1)",
           conversation_id);
    
    SPI_prepare(insert_sql, 1, NULL);
    Datum values[1];
    values[0] = CStringGetTextDatum(message);
    
    SPI_execute_plan(SPI_prepare_cursor(insert_sql, 1, NULL, "message"), values, NULL, false, 0);
    
    SPI_finish();
    
    // Construct conversation history
    int total_length = 0;
    for (int i = 0; i < cache->message_count; i++) {
        total_length += strlen(cache->roles[i]) + strlen(cache->messages[i]) + 50;
    }
    
    char *history = palloc(total_length);
    history[0] = '\0';
    
    for (int i = 0; i < cache->message_count; i++) {
        strcat(history, cache->roles[i]);
        strcat(history, ": ");
        strcat(history, cache->messages[i]);
        strcat(history, "\n");
    }
    
    // Call model to get response
    const char *system_message = "You are a PostgreSQL database assistant. You can help users analyze data, optimize queries, "
                               "generate reports and visualizations. Keep answers concise and professional.";
    
    ModelResponse *response = call_model("chatgpt", history, system_message);
    
    char *result;
    if (response->successful) {
        result = pstrdup(response->response);
        
        // Add assistant response to cache
        if (cache->message_count >= cache->capacity) {
            cache->capacity *= 2;
            cache->messages = repalloc(cache->messages, sizeof(char*) * cache->capacity);
            cache->roles = repalloc(cache->roles, sizeof(char*) * cache->capacity);
        }
        
        cache->roles[cache->message_count] = pstrdup("assistant");
        cache->messages[cache->message_count] = pstrdup(result);
        cache->message_count++;
        
        // Store assistant response in database
        SPI_connect();
        
        char assistant_sql[1000];
        sprintf(assistant_sql, "INSERT INTO pg_llm_messages (conversation_id, role, content) VALUES (%d, 'assistant', $1)",
               conversation_id);
        
        SPI_prepare(assistant_sql, 1, NULL);
        values[0] = CStringGetTextDatum(result);
        
        SPI_execute_plan(SPI_prepare_cursor(assistant_sql, 1, NULL, "assistant"), values, NULL, false, 0);
        
        SPI_finish();
    } else {
        result = pstrdup("Failed to get response");
    }
    
    pfree(history);
    pfree(response);
    
    return result;
}

// Create new conversation
int create_conversation(void)
{
    // Generate new ID
    int new_id = 1;
    
    SPI_connect();
    
    // Get maximum ID
    int ret = SPI_execute("SELECT COALESCE(MAX(id) + 1, 1) AS new_id FROM pg_llm_conversations", true, 0);
    
    if (ret == SPI_OK_SELECT && SPI_processed > 0) {
        char *id_str = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
        new_id = atoi(id_str);
    }
    
    // Create new session
    char sql[100];
    sprintf(sql, "INSERT INTO pg_llm_conversations (id) VALUES (%d)", new_id);
    SPI_execute(sql, false, 0);
    
    SPI_finish();
    
    return new_id;
}

// Delete conversation
bool delete_conversation(int conversation_id)
{
    SPI_connect();
    
    char sql[100];
    sprintf(sql, "DELETE FROM pg_llm_conversations WHERE id = %d", conversation_id);
    int ret = SPI_execute(sql, false, 0);
    
    SPI_finish();
    
    // Delete cache
    for (int i = 0; i < cache_count; i++) {
        if (conversation_caches[i]->id == conversation_id) {
            ConversationCache *cache = conversation_caches[i];
            
            for (int j = 0; j < cache->message_count; j++) {
                pfree(cache->messages[j]);
                pfree(cache->roles[j]);
            }
            
            pfree(cache->messages);
            pfree(cache->roles);
            pfree(cache);
            
            // Move subsequent caches
            memmove(&conversation_caches[i], &conversation_caches[i+1], 
                   sizeof(ConversationCache*) * (cache_count - i - 1));
            cache_count--;
            break;
        }
    }
    
    return (ret == SPI_OK_DELETE);
}

// Get conversation history
char* get_conversation_history(int conversation_id)
{
    // Find conversation cache
    ConversationCache *cache = NULL;
    for (int i = 0; i < cache_count; i++) {
        if (conversation_caches[i]->id == conversation_id) {
            cache = conversation_caches[i];
            break;
        }
    }
    
    // If not in cache, try to load from database
    if (!cache) {
        SPI_connect();
        
        // Check if conversation exists
        char check_sql[100];
        sprintf(check_sql, "SELECT 1 FROM pg_llm_conversations WHERE id = %d", conversation_id);
        
        int ret = SPI_execute(check_sql, true, 0);
        if (ret != SPI_OK_SELECT || SPI_processed == 0) {
            SPI_finish();
            return pstrdup("Conversation not found");
        }
        
        // Get messages
        char query_sql[200];
        sprintf(query_sql, 
                "SELECT role, content FROM pg_llm_messages WHERE conversation_id = %d ORDER BY created_at",
                conversation_id);
        
        ret = SPI_execute(query_sql, true, 0);
        if (ret != SPI_OK_SELECT) {
            SPI_finish();
            return pstrdup("Failed to retrieve conversation history");
        }
        
        // Create new cache entry
        cache = palloc(sizeof(ConversationCache));
        cache->id = conversation_id;
        cache->capacity = SPI_processed > 10 ? SPI_processed : 10;
        cache->messages = palloc(sizeof(char*) * cache->capacity);
        cache->roles = palloc(sizeof(char*) * cache->capacity);
        cache->message_count = SPI_processed;
        
        // Copy messages to cache
        for (int i = 0; i < SPI_processed; i++) {
            HeapTuple tuple = SPI_tuptable->vals[i];
            TupleDesc tupdesc = SPI_tuptable->tupdesc;
            
            char *role = SPI_getvalue(tuple, tupdesc, 1);
            char *content = SPI_getvalue(tuple, tupdesc, 2);
            
            cache->roles[i] = pstrdup(role);
            cache->messages[i] = pstrdup(content);
        }
        
        // Add to cache array
        if (cache_count >= cache_capacity) {
            cache_capacity *= 2;
            conversation_caches = repalloc(conversation_caches, 
                                         sizeof(ConversationCache*) * cache_capacity);
        }
        conversation_caches[cache_count++] = cache;
        
        SPI_finish();
    }
    
    // Format conversation history
    StringInfoData history;
    initStringInfo(&history);
    
    appendStringInfo(&history, "{\n");
    appendStringInfo(&history, "  \"conversation_id\": %d,\n", conversation_id);
    appendStringInfo(&history, "  \"messages\": [\n");
    
    for (int i = 0; i < cache->message_count; i++) {
        appendStringInfo(&history, "    {\n");
        appendStringInfo(&history, "      \"role\": \"%s\",\n", cache->roles[i]);
        appendStringInfo(&history, "      \"content\": \"%s\"\n", cache->messages[i]);
        appendStringInfo(&history, "    }%s\n", i < cache->message_count - 1 ? "," : "");
    }
    
    appendStringInfo(&history, "  ]\n");
    appendStringInfo(&history, "}\n");
    
    return history.data;
} 