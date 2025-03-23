#include "models/qianwen_model.h"

namespace pg_llm {

QianwenModel::QianwenModel() : LLMInterface() {
}

QianwenModel::~QianwenModel() {
}

bool QianwenModel::initialize(const std::string& api_key, const std::string& model_config) {
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

  model_name_ = config.get("model_name", "qwen-turbo").asString();
  api_endpoint_ = config.get("api_endpoint", "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation").asString();
  access_key_id_ = config.get("access_key_id", "").asString();
  access_key_secret_ = config.get("access_key_secret", "").asString();

  if (access_key_id_.empty() || access_key_secret_.empty()) {
    return false;
  }

  is_initialized_ = true;
  return true;
}

ModelResponse QianwenModel::chat_completion(const std::string& prompt) {
  std::vector<ChatMessage> messages = {{"user", prompt}};
  return chat_completion(messages);
}

ModelResponse QianwenModel::chat_completion(const std::vector<ChatMessage>& messages) {
  if (!is_ready()) {
    return ModelResponse{"Model not initialized", 0.0f, get_model_name()};
  }


  // Prepare request body
  Json::Value request_body;
  request_body["model"] = model_name_;
  Json::Value message_arr(Json::arrayValue);

  // Process all messages in the conversation
  for (const auto& msg : messages) {
    Json::Value message;
    message["role"] = msg.role;
    message["content"] = msg.content;
    message_arr.append(message);
  }

  request_body["messages"] = message_arr;
  request_body["stream"] = false;
  // request_body["parameters"]["temperature"] = 0.7;
  // request_body["parameters"]["top_p"] = 0.9;
  // request_body["parameters"]["result_format"] = "text";

  // Serializing the request body
  Json::StreamWriterBuilder writer_builder;
  std::string request_body_str = Json::writeString(writer_builder, request_body);

  ResponseData response_data;
  // Make API request with signature
  CURLcode res = make_api_request(api_endpoint_, request_body_str, response_data);
  if (res != CURLE_OK) {
    // TODO(yanghao): add error handling
    // std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
    return ModelResponse{"Failed to make API request", 0.0f, get_model_name()};
  } else {
    long http_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code == 200) {
      Json::CharReaderBuilder reader_builder;
      std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
      Json::Value response_json;
      std::string parse_errors;

      const char* contents_tart = response_data.content.c_str();
      if (reader->parse(contents_tart,
                        contents_tart + response_data.content.size(),
                        &response_json, &parse_errors)) {
        if (response_json.isMember("choices") &&
            response_json["choices"].isArray() &&
            !response_json["choices"].empty()) {
          Json::Value& first_choice = response_json["choices"][0u];
          if (first_choice.isMember("message") &&
              first_choice["message"].isMember("content") &&
              first_choice["message"]["content"].isString()) {
            response_data.fullReply = first_choice["message"]["content"].asString();
            PG_LLM_LOG_INFO("Complete reply: %s", response_data.fullReply.c_str());
            // Only return the content field, not the entire JSON response
            return ModelResponse{response_data.fullReply, 0.9, get_model_name()};
          }
        } else {
          std::cerr << "Response format exception: missing choices field" << std::endl;
        }
      } else {
        std::cerr << "JSON parsing failed: " << parse_errors << std::endl;
        std::cerr << "Raw response: " << response_data.content << std::endl;
      }
    } else {
      std::cerr << "HTTP error: " << http_code << std::endl;
      std::cerr << "Error response: " << response_data.content << std::endl;
    }
  }

  // If extraction fails, return the entire original response
  return ModelResponse{response_data.content, 0.9, get_model_name()};
}

std::string QianwenModel::get_model_name() const {
  return model_name_;
}

std::string QianwenModel::get_model_info() const {
  return "Alibaba Qianwen Model - " + model_name_;
}

bool QianwenModel::is_ready() const {
  return is_initialized_ && curl_ && !access_key_id_.empty() && !access_key_secret_.empty();
}

CURLcode QianwenModel::make_api_request(const std::string& endpoint,
                                        const std::string& request_body,
                                        ResponseData &response_data) {
  // TODO(yanghao): add error handling
  if (!curl_) {
    return CURLE_FAILED_INIT;
  }

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
    // TODO(yanghao): add error handling
    return res;
  }

  return CURLE_OK;
}

std::vector<float> QianwenModel::get_embedding(const std::string& text) {
  if (!is_ready()) {
    throw std::runtime_error("Model is not initialized");
  }

  // Prepare request body
  Json::Value request_body;
  request_body["input"] = text;
  request_body["model"] = "qwen-embedding";  // Use Qianwen's embedding model

  // Serialize request body
  Json::StreamWriterBuilder writer_builder;
  std::string request_body_str = Json::writeString(writer_builder, request_body);

  // Make API request
  ResponseData response_data;
  std::string embedding_endpoint = "https://dashscope.aliyuncs.com/api/v1/embeddings";
  CURLcode res = make_api_request(embedding_endpoint, request_body_str, response_data);

  if (res != CURLE_OK) {
    throw std::runtime_error("Failed to get embedding: " + std::string(curl_easy_strerror(res)));
  }

  // Parse response
  Json::Value response_json;
  Json::CharReaderBuilder reader_builder;
  std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
  std::string parse_errors;

  if (!reader->parse(response_data.content.c_str(),
                     response_data.content.c_str() + response_data.content.size(),
                     &response_json, &parse_errors)) {
    throw std::runtime_error("Failed to parse embedding response: " + parse_errors);
  }

  // Extract embedding vector
  if (response_json.isMember("output") &&
      response_json["output"].isMember("embeddings") &&
      response_json["output"]["embeddings"].isArray() &&
      !response_json["output"]["embeddings"].empty()) {

    std::vector<float> embedding;
    const Json::Value& embeddings = response_json["output"]["embeddings"][0];
    for (const auto& value : embeddings) {
      embedding.push_back(value.asFloat());
    }
    return embedding;
  }

  throw std::runtime_error("Invalid embedding response format");
}

} // namespace pg_llm