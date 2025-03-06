#include "../../include/model_interface.h"
#include <json-c/json.h>

// Get access token
static char* get_access_token(const char *api_key, const char *secret_key)
{
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    char *access_token = NULL;
    
    chunk.memory = palloc(1);
    chunk.size = 0;
    
    curl = curl_easy_init();
    if (curl) {
        char url[512];
        snprintf(url, sizeof(url), 
                "https://aip.baidubce.com/oauth/2.0/token?"
                "grant_type=client_credentials&client_id=%s&client_secret=%s",
                api_key, secret_key);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            struct json_object *resp_json = json_tokener_parse(chunk.memory);
            if (resp_json) {
                struct json_object *token_obj;
                if (json_object_object_get_ex(resp_json, "access_token", &token_obj)) {
                    access_token = pstrdup(json_object_get_string(token_obj));
                }
                json_object_put(resp_json);
            }
        }
        
        curl_easy_cleanup(curl);
    }
    
    pfree(chunk.memory);
    return access_token;
}

// Call Baidu Wenxin model
ModelResponse* call_wenxin_model(const char *prompt, const char *system_message,
                                const char *api_key, const char *secret_key)
{
    // Get access token
    char *access_token = get_access_token(api_key, secret_key);
    if (!access_token) {
        ModelResponse *response = palloc(sizeof(ModelResponse));
        response->successful = false;
        response->error_message = pstrdup("Failed to get access token");
        return response;
    }
    
    struct json_object *root = json_object_new_object();
    
    // Create message array
    struct json_object *messages = create_message_array(system_message, prompt);
    json_object_object_add(root, "messages", messages);
    json_object_object_add(root, "temperature", json_object_new_double(0.7));
    
    // Construct URL
    char url[512];
    snprintf(url, sizeof(url), 
            "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat/completions?"
            "access_token=%s",
            access_token);
    
    // Use common HTTP request function
    ModelResponse *response = make_http_request(
        url,
        NULL,  // Wenxin doesn't need API key in request header
        json_object_to_json_string(root),
        10000  // 10 seconds timeout
    );
    
    // Cleanup resources
    json_object_put(root);
    pfree(access_token);
    
    return response;
} 