#include "../include/model_interface.h"
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Model information storage
typedef struct {
    char *name;
    char *api_key;
    char *api_url;
    char *version;
    int timeout_ms;
    int max_tokens;
    bool enabled;
} ModelConfig;

// Model management
static ModelConfig **registered_models = NULL;
static int model_count = 0;

// Model handler array
static ModelHandler model_handlers[] = {
    {"chatgpt", call_chatgpt_model},
    {"qianwen", call_qianwen_model},
    {"wenxin", call_wenxin_model},
    {"doubao", call_doubao_model},
    {"deepseek", call_deepseek_model},
    {"local_model", call_local_model}
};
static const int handler_count = sizeof(model_handlers) / sizeof(ModelHandler);

// Initialize model interface
void initialize_model_interface(void)
{
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Add default model configuration
    registered_models = palloc(sizeof(ModelConfig*) * handler_count);
    model_count = handler_count;
    
    // Initialize configuration for each model
    for (int i = 0; i < handler_count; i++) {
        registered_models[i] = palloc(sizeof(ModelConfig));
        registered_models[i]->name = pstrdup(model_handlers[i].name);
        registered_models[i]->enabled = false;
        registered_models[i]->timeout_ms = 10000;
        registered_models[i]->max_tokens = 1000;
    }
}

// Clean up model interface
void cleanup_model_interface(void)
{
    for (int i = 0; i < model_count; i++) {
        pfree(registered_models[i]->name);
        pfree(registered_models[i]->api_key);
        pfree(registered_models[i]->api_url);
        pfree(registered_models[i]->version);
        pfree(registered_models[i]);
    }
    
    pfree(registered_models);
    curl_global_cleanup();
}

// Call model API
ModelResponse* call_model(const char *model_name, const char *prompt, const char *system_message)
{
    // Find model handler
    ModelHandler *handler = NULL;
    for (int i = 0; i < handler_count; i++) {
        if (strcmp(model_handlers[i].name, model_name) == 0) {
            handler = &model_handlers[i];
            break;
        }
    }
    
    if (!handler) {
        ModelResponse *response = palloc(sizeof(ModelResponse));
        response->successful = false;
        response->error_message = pstrdup("Model not found");
        return response;
    }
    
    // Get model configuration
    ModelConfig *config = get_model_config(model_name);
    if (!config || !config->enabled) {
        ModelResponse *response = palloc(sizeof(ModelResponse));
        response->successful = false;
        response->error_message = pstrdup("Model not enabled or invalid configuration");
        return response;
    }
    
    // Call model handler function
    return handler->call_model(prompt, system_message, config->api_key, config->api_url);
}

// Get available models list
char** get_available_models(int *count)
{
    *count = 0;
    char **models = palloc(sizeof(char*) * model_count);
    
    for (int i = 0; i < model_count; i++) {
        if (registered_models[i]->enabled) {
            models[*count] = pstrdup(registered_models[i]->name);
            (*count)++;
        }
    }
    
    return models;
}

// Configure plugin
bool configure_plugin(const char *config_json)
{
    struct json_object *root = json_tokener_parse(config_json);
    if (!root) {
        return false;
    }
    
    struct json_object *models;
    if (json_object_object_get_ex(root, "models", &models)) {
        json_object_object_foreach(models, key, val) {
            // Find model configuration
            ModelConfig *config = get_model_config(key);
            if (config) {
                // Update configuration
                struct json_object *api_key, *api_url, *enabled;
                if (json_object_object_get_ex(val, "api_key", &api_key)) {
                    config->api_key = pstrdup(json_object_get_string(api_key));
                }
                if (json_object_object_get_ex(val, "api_url", &api_url)) {
                    config->api_url = pstrdup(json_object_get_string(api_url));
                }
                if (json_object_object_get_ex(val, "enabled", &enabled)) {
                    config->enabled = json_object_get_boolean(enabled);
                }
            }
        }
    }
    
    json_object_put(root);
    return true;
} 