#ifndef PG_LLM_MODEL_INTERFACE_H
#define PG_LLM_MODEL_INTERFACE_H

#include "postgres.h"

/* Model configuration structure */
typedef struct {
    char *name;
    char *api_key;
    char *api_url;
    char *version;
    int timeout_ms;
    int max_tokens;
    bool enabled;
} ModelConfig;

/* Model response structure */
typedef struct {
    bool successful;
    char *response;
    char *error_message;
    float confidence;
} ModelResponse;

/* Model handler structure */
typedef struct {
    char *name;
    ModelResponse* (*call_model)(const char *prompt, const char *system_message,
                                const char *api_key, const char *api_url);
} ModelHandler;

/* Common function declarations */
ModelResponse* make_http_request(const char *api_url, const char *api_key, 
                                const char *json_data, int timeout_ms);
struct json_object* create_message_array(const char *system_message, const char *prompt);
ModelConfig* get_model_config(const char *model_name);

/* Model function declarations */
ModelResponse* call_chatgpt_model(const char *prompt, const char *system_message,
                                 const char *api_key, const char *api_url);
ModelResponse* call_qianwen_model(const char *prompt, const char *system_message,
                                 const char *api_key, const char *api_url);
ModelResponse* call_wenxin_model(const char *prompt, const char *system_message,
                                const char *api_key, const char *api_url);
ModelResponse* call_doubao_model(const char *prompt, const char *system_message,
                                const char *api_key, const char *api_url);
ModelResponse* call_deepseek_model(const char *prompt, const char *system_message,
                                  const char *api_key, const char *api_url);
ModelResponse* call_local_model(const char *prompt, const char *system_message,
                               const char *api_key, const char *api_url);

#endif /* PG_LLM_MODEL_INTERFACE_H */ 