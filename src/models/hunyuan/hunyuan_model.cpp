#include "models/hunyuan_model.h"

namespace pg_llm {

HunyuanModel::HunyuanModel() : LLMInterface() {
}

HunyuanModel::~HunyuanModel() {
}

bool HunyuanModel::initialize(const std::string& api_key, const std::string& model_config) {
    if (!curl_) {
        return false;
    }

    api_key_ = api_key;
    
    // Parse model configuration
    Json::Value config;
    Json::Reader reader;
    if (!reader.parse(model_config, config)) {
        return false;
    }

    model_name_ = config.get("model_name", "hunyuan").asString();
    api_endpoint_ = config.get("api_endpoint", "https://hunyuan.cloud.tencent.com/hyllm/v1/chat/completions").asString();
    access_key_id_ = config.get("secret_key", "").asString();

    if (access_key_id_.empty()) {
        return false;
    }

    is_initialized_ = true;
    return true;
}

ModelResponse HunyuanModel::chat_completion(const std::string& prompt) {
    std::vector<ChatMessage> messages = {{"user", prompt}};
    return chat_completion(messages);
}

ModelResponse HunyuanModel::chat_completion(const std::vector<ChatMessage>& messages) {
    if (!is_ready()) {
        return ModelResponse{"Model not initialized", 0.0f, get_model_name()};
    }
    
    // Prepare request body
    Json::Value request_body;
    request_body["model"] = model_name_;
    Json::Value message_arr(Json::arrayValue);
    for (const auto& msg : messages) {
        Json::Value message;
        message["role"] = msg.role;
        message["content"] = msg.content;
        message_arr.append(message);
    }
    request_body["messages"] = message_arr;
    request_body["stream"] = false;
    request_body["temperature"] = 0.7;

    // Serializing the request body
    Json::StreamWriterBuilder writer_builder;
    std::string request_body_str = Json::writeString(writer_builder, request_body);

    ResponseData response_data;
    // Make API request
    CURLcode res = make_api_request(api_endpoint_, request_body_str, response_data);
    if (res != CURLE_OK) {
        return ModelResponse{"Failed to make API request", 0.0f, get_model_name()};
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200) {
            Json::CharReaderBuilder reader_builder;
            std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
            Json::Value response_json;
            std::string parse_errors;

            const char* contents_start = response_data.content.c_str();
            if (reader->parse(contents_start,
                              contents_start + response_data.content.size(),
                              &response_json, &parse_errors)) {
                if (response_json.isMember("choices") &&
                    response_json["choices"].isArray() &&
                    !response_json["choices"].empty()) {
                    Json::Value& first_choice = response_json["choices"][0u];
                    if (first_choice.isMember("message") &&
                        first_choice["message"].isMember("content") &&
                        first_choice["message"]["content"].isString()) {
                        response_data.fullReply = first_choice["message"]["content"].asString();
                        // Only return the content field, not the entire JSON response
                        return ModelResponse{response_data.fullReply, 0.9, get_model_name()};
                    }
                }
            }
        }
    }
    
    // If extraction fails, return the entire original response
    return ModelResponse{response_data.content, 0.7, get_model_name()};
}

std::string HunyuanModel::get_model_name() const {
    return model_name_;
}

std::string HunyuanModel::get_model_info() const {
    return "Tencent Hunyuan Model - " + model_name_;
}

bool HunyuanModel::is_ready() const {
    return is_initialized_ && curl_ && !access_key_id_.empty();
}

CURLcode HunyuanModel::make_api_request(const std::string& endpoint,
                                        const std::string& request_body,
                                        ResponseData &response_data) {
    if (!curl_) {
        return CURLE_FAILED_INIT;
    }

    // Reset response content
    response_data.content.clear();
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key_).c_str());

    curl_easy_setopt(curl_, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, request_body.length());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);

    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        return res;
    }

    return CURLE_OK;
}

} // namespace pg_llm 