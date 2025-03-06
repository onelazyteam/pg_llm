#include "../../include/model_interface.h"
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Response data structure
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Callback function to receive HTTP response data
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    char *ptr = repalloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        return 0;
    }
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

// Common HTTP request handling function
ModelResponse* make_http_request(const char *api_url, const char *api_key, 
                                const char *json_data, int timeout_ms)
{
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    ModelResponse *response = palloc(sizeof(ModelResponse));
    memset(response, 0, sizeof(ModelResponse));
    
    chunk.memory = palloc(1);
    chunk.size = 0;
    
    curl = curl_easy_init();
    if (curl) {
        // Set request headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
        headers = curl_slist_append(headers, auth_header);
        
        // Set request options
        curl_easy_setopt(curl, CURLOPT_URL, api_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
        
        // Execute request
        res = curl_easy_perform(curl);
        
        // Check results
        if (res != CURLE_OK) {
            response->successful = false;
            response->error_message = pstrdup(curl_easy_strerror(res));
        } else {
            // Parse JSON response
            struct json_object *resp_json = json_tokener_parse(chunk.memory);
            if (resp_json) {
                struct json_object *choices, *msg, *content;
                if (json_object_object_get_ex(resp_json, "choices", &choices) &&
                    json_object_array_length(choices) > 0) {
                    
                    struct json_object *first_choice = json_object_array_get_idx(choices, 0);
                    if (json_object_object_get_ex(first_choice, "message", &msg) &&
                        json_object_object_get_ex(msg, "content", &content)) {
                        
                        response->successful = true;
                        response->response = pstrdup(json_object_get_string(content));
                        response->confidence = 0.9; // Default confidence
                    } else {
                        response->successful = false;
                        response->error_message = pstrdup("Failed to parse response content");
                    }
                } else {
                    response->successful = false;
                    response->error_message = pstrdup("Invalid API response");
                }
                
                json_object_put(resp_json);
            } else {
                response->successful = false;
                response->error_message = pstrdup("Failed to parse JSON response");
            }
        }
        
        // Cleanup resources
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        response->successful = false;
        response->error_message = pstrdup("Failed to initialize CURL");
    }
    
    pfree(chunk.memory);
    return response;
}

// Create message array
struct json_object* create_message_array(const char *system_message, const char *prompt)
{
    struct json_object *messages = json_object_new_array();
    
    // Add system message
    if (system_message && system_message[0] != '\0') {
        struct json_object *sys_msg = json_object_new_object();
        json_object_object_add(sys_msg, "role", json_object_new_string("system"));
        json_object_object_add(sys_msg, "content", json_object_new_string(system_message));
        json_object_array_add(messages, sys_msg);
    }
    
    // Add user message
    struct json_object *user_msg = json_object_new_object();
    json_object_object_add(user_msg, "role", json_object_new_string("user"));
    json_object_object_add(user_msg, "content", json_object_new_string(prompt));
    json_object_array_add(messages, user_msg);
    
    return messages;
}

// Get model configuration
ModelConfig* get_model_config(const char *model_name)
{
    // Get model configuration from global configuration
    extern ModelConfig **registered_models;
    extern int model_count;
    
    for (int i = 0; i < model_count; i++) {
        if (strcmp(registered_models[i]->name, model_name) == 0) {
            return registered_models[i];
        }
    }
    return NULL;
} 