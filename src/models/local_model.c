#include "../../include/model_interface.h"
#include <string.h>
#include <llama.h>  // Using llama.cpp as local model

// Local model configuration
typedef struct {
    struct llama_context *ctx;
    char *model_path;
    int max_tokens;
    float temperature;
    bool initialized;
} LocalModelConfig;

static LocalModelConfig config = {0};

// Initialize local model
static bool initialize_local_model(const char *model_path)
{
    if (config.initialized) {
        return true;
    }
    
    // Initialize llama.cpp
    struct llama_context_params params = llama_context_default_params();
    params.n_ctx = 2048;
    params.n_threads = 4;
    
    config.ctx = llama_init_from_file(model_path, params);
    if (!config.ctx) {
        return false;
    }
    
    config.model_path = pstrdup(model_path);
    config.max_tokens = 1000;
    config.temperature = 0.7;
    config.initialized = true;
    
    return true;
}

// Cleanup local model
static void cleanup_local_model(void)
{
    if (config.initialized) {
        llama_free(config.ctx);
        pfree(config.model_path);
        memset(&config, 0, sizeof(LocalModelConfig));
    }
}

// Call local model
ModelResponse* call_local_model(const char *prompt, const char *system_message,
                              const char *api_key, const char *api_url)
{
    ModelResponse *response = palloc(sizeof(ModelResponse));
    memset(response, 0, sizeof(ModelResponse));
    
    // Check if model is initialized
    if (!config.initialized) {
        // Use api_url as model path if provided, otherwise use default
        const char *model_path = api_url ? api_url : "/usr/local/share/pg_llm/models/local_model.bin";
        if (!initialize_local_model(model_path)) {
            response->successful = false;
            response->error_message = pstrdup("Failed to initialize local model");
            return response;
        }
    }
    
    // Construct complete prompt
    char *full_prompt;
    if (system_message && system_message[0] != '\0') {
        full_prompt = palloc(strlen(system_message) + strlen(prompt) + 10);
        sprintf(full_prompt, "%s\n%s", system_message, prompt);
    } else {
        full_prompt = pstrdup(prompt);
    }
    
    // Generation parameters
    struct llama_sampling_params sampling = {
        .temperature = config.temperature,
        .top_p = 0.95f,
        .top_k = 40,
        .tokens_max = config.max_tokens,
    };
    
    // Generate response
    char *result = palloc(config.max_tokens * 4); // Assume max 4 bytes per token
    size_t result_len = 0;
    
    llama_sampling_context *ctx_sampling = llama_sampling_init(sampling);
    
    // Process prompt
    llama_batch batch = llama_batch_init(512, 0, 1);
    llama_tokenize(config.ctx, full_prompt, strlen(full_prompt), &batch, true);
    
    if (llama_decode(config.ctx, batch) > 0) {
        // Generate response
        while (result_len < config.max_tokens) {
            llama_token token = llama_sampling_sample(ctx_sampling, config.ctx);
            if (token == llama_token_eos()) {
                break;
            }
            
            const char *text = llama_token_to_str(config.ctx, token);
            size_t text_len = strlen(text);
            
            if (result_len + text_len >= config.max_tokens * 4) {
                break;
            }
            
            memcpy(result + result_len, text, text_len);
            result_len += text_len;
        }
    }
    
    result[result_len] = '\0';
    
    // Cleanup
    llama_sampling_free(ctx_sampling);
    llama_batch_free(batch);
    pfree(full_prompt);
    
    // Set response
    response->successful = true;
    response->response = pstrdup(result);
    response->confidence = 0.8; // Default confidence for local model
    
    pfree(result);
    
    return response;
}

// Configure local model
bool configure_local_model(const char *config_json)
{
    if (!config_json) {
        return false;
    }
    
    struct json_object *root = json_tokener_parse(config_json);
    if (!root) {
        return false;
    }
    
    bool success = false;
    struct json_object *model_path, *max_tokens, *temperature;
    
    if (json_object_object_get_ex(root, "model_path", &model_path)) {
        const char *path = json_object_get_string(model_path);
        success = initialize_local_model(path);
    }
    
    if (json_object_object_get_ex(root, "max_tokens", &max_tokens)) {
        config.max_tokens = json_object_get_int(max_tokens);
    }
    
    if (json_object_object_get_ex(root, "temperature", &temperature)) {
        config.temperature = json_object_get_double(temperature);
    }
    
    json_object_put(root);
    return success;
} 