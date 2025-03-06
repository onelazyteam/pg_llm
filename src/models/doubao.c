#include "../../include/model_interface.h"
#include <json-c/json.h>

// Call ByteDance Doubao model
ModelResponse* call_doubao_model(const char *prompt, const char *system_message,
                               const char *api_key, const char *api_url)
{
    struct json_object *root = json_object_new_object();
    
    // Create message array
    struct json_object *messages = create_message_array(system_message, prompt);
    json_object_object_add(root, "messages", messages);
    json_object_object_add(root, "model", json_object_new_string("doubao-text"));
    json_object_object_add(root, "temperature", json_object_new_double(0.7));
    json_object_object_add(root, "max_tokens", json_object_new_int(1000));
    
    // Use common HTTP request function
    ModelResponse *response = make_http_request(
        api_url,
        api_key,
        json_object_to_json_string(root),
        10000  // 10 seconds timeout
    );
    
    // Cleanup JSON object
    json_object_put(root);
    
    return response;
} 