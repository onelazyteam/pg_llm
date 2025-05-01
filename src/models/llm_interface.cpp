#include "models/llm_interface.h"

namespace pg_llm {
bool LLMInterface::initialize(bool local_model,
  const std::string& api_key,
  const std::string& model_config) {
  if (!curl_) {
    PG_LLM_LOG_ERROR("model:%s init failed, both apikey and access_key is invalid.", model_type_.c_str());
    return false;
  }

  local_model_ = local_model;
  api_key_ = api_key;

  // Parse model configuration
  Json::Value config;
  Json::Reader reader;
  if (!reader.parse(model_config, config)) {
    PG_LLM_LOG_WARNING("model:%s parse config info failed.", model_type_.c_str());
    return false;
  }

  model_name_ = config.get("model_name", "").asString();
  api_endpoint_ = config.get("api_endpoint", "").asString();
  access_key_id_ = config.get("access_key_id", "").asString();
  access_key_secret_ = config.get("access_key_secret", "").asString();

  if (!local_model && api_key_.empty() && (access_key_id_.empty() || access_key_secret_.empty())) {
    PG_LLM_LOG_ERROR("model:%s init failed, both apikey and access_key is invalid.", model_type_.c_str());
    return false;
  }

  is_initialized_ = true;
  return true;
}

ModelResponse LLMInterface::chat_completion(const std::string& prompt) {
  std::vector<ChatMessage> messages = {{"user", prompt}};
  return chat_completion(messages);
}

ModelResponse LLMInterface::chat_completion(const std::vector<ChatMessage>& messages) {
  if (!is_ready()) {
    PG_LLM_LOG_ERROR("model:%s not initialized.", model_type_.c_str());
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
  request_body["parameters"]["temperature"] = 0.6;
  request_body["parameters"]["top_p"] = 0.9;
  request_body["parameters"]["logprobs"] = 1;

  // Serializing the request body
  Json::StreamWriterBuilder writer_builder;
  std::string request_body_str = Json::writeString(writer_builder, request_body);

  ResponseData response_data;
  // Make API request with signature
  CURLcode res = make_api_request(api_endpoint_, request_body_str, response_data);
  if (res != CURLE_OK) {
    PG_LLM_LOG_ERROR("Failed to make API request");
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
        // get confidence score
        double confidence = 0;
        if (response_json.isMember("usage") &&
            response_json["choices"].isArray() &&
            !response_json["choices"].empty()) {
            double total = response_json["usage"]["total_tokens"].asDouble();
            double output = response_json["usage"]["output_tokens"].asDouble();
            confidence = (total > 0) ? (output / total) : 0.0;
            PG_LLM_LOG_INFO("confidence: %lf", confidence);
        }

        // get question answer
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
            return ModelResponse{response_data.fullReply, confidence, get_model_name()};
          }
        } else {
          PG_LLM_LOG_ERROR("Response format exception: missing choices field");
        }
      } else {
        PG_LLM_LOG_ERROR("JSON parsing failed: %s, Raw response: %s",
          parse_errors.c_str(), response_data.content.c_str());
      }
    } else {
      PG_LLM_LOG_ERROR("HTTP error: %ld, Error response: %s",
        http_code, response_data.content.c_str());
    }
  }

  // If extraction fails, return the entire original response
  return ModelResponse{response_data.content, 0.9, get_model_name()};
}

std::string LLMInterface::get_model_name() const {
  return model_name_;
}

std::string LLMInterface::get_model_info() const {
  return "LLM Model - " + model_name_;
}

bool LLMInterface::is_ready() const {
  bool is_ready = true;
  if (unlikely(local_model_)) {
    is_ready = (is_initialized_ && curl_ && !api_endpoint_.empty());
  } else {
    is_ready = is_initialized_ && curl_ && (!access_key_id_.empty() && !access_key_secret_.empty());
  }
  return is_ready;
}

CURLcode LLMInterface::make_api_request(const std::string& endpoint,
                                        const std::string& request_body,
                                        ResponseData &response_data) {
  if (!curl_) {
    return CURLE_FAILED_INIT;
  }

  struct curl_slist* headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if (unlikely(local_model_)) {
    headers = curl_slist_append(headers, "Authorization: Bearer ollama");
  } else {
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key_).c_str());
  }

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
    PG_LLM_LOG_ERROR("curl get response field");
    return res;
  }

  return CURLE_OK;
}

std::vector<float> LLMInterface::get_embedding(const std::string& text) {
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
    PG_LLM_LOG_ERROR("Failed to get embedding: %s", curl_easy_strerror(res));
  }

  // Parse response
  Json::Value response_json;
  Json::CharReaderBuilder reader_builder;
  std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
  std::string parse_errors;

  if (!reader->parse(response_data.content.c_str(),
                     response_data.content.c_str() + response_data.content.size(),
                     &response_json, &parse_errors)) {
    PG_LLM_LOG_ERROR("Failed to parse embedding response: %s", parse_errors.c_str());
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

  PG_LLM_LOG_ERROR("Invalid embedding response format");
  return std::vector<float>();
}

// Streaming callback function (processes data chunk by chunk)
size_t LLMInterface::stream_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t realsize = size * nmemb;
  StreamContext* ctx = static_cast<StreamContext*>(userp);
  ctx->buffer.append(static_cast<char*>(contents), realsize);

  // Process data line by line
  size_t pos = 0;
  while ((pos = ctx->buffer.find("\n")) != std::string::npos) {
    std::string line = ctx->buffer.substr(0, pos);
    ctx->buffer.erase(0, pos + 1);

    // Skip empty lines and "[DONE]"
    if (line.empty() || line == "data: [DONE]") continue;

    // Extract valid JSON data
    if (line.find("data: ") == 0) { // Check prefix
      line = line.substr(6); // Remove "data: " prefix

      // Parse JSON using jsoncpp
      Json::Value chunk;
      Json::CharReaderBuilder readerBuilder;
      std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
      const char* lineStart = line.c_str();
      std::string parseErrors;

      if (reader->parse(lineStart, lineStart + line.size(), &chunk, &parseErrors)) {
        // Check if choices array exists and is not empty
        if (chunk.isMember("choices") && chunk["choices"].isArray() && !chunk["choices"].empty()) {
          Json::Value& delta = chunk["choices"][0u]["delta"]; // Use unsigned index

          // Check if delta contains content field
          if (delta.isMember("content") && delta["content"].isString()) {
            std::string content = delta["content"].asString();
            if (!content.empty()) {
              ctx->fullReply += content;
              std::cout << content << std::flush; // Real-time output
            }
          }
        }
      } else {
        PG_LLM_LOG_ERROR("JSON parsing error: %s", parseErrors.c_str());
      }
    }
  }

  return realsize;
}

std::string LLMInterface::generate_signature(const std::string& request_body) {
  // Generate timestamp
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                     now.time_since_epoch()).count();

  // Prepare string to sign
  std::stringstream string_to_sign;
  string_to_sign << "POST\n"
                 << api_endpoint_ << "\n"
                 << timestamp << "\n"
                 << request_body;

  // Generate HMAC-SHA256 signature
  unsigned char* hmac = HMAC(EVP_sha256(),
                             access_key_secret_.c_str(), access_key_secret_.length(),
                             (unsigned char*)string_to_sign.str().c_str(),
                             string_to_sign.str().length(),
                             NULL, NULL);

  // Convert to Base64
  BIO* b64 = BIO_new(BIO_f_base64());
  BIO* bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
  BIO_write(b64, hmac, 32);
  BIO_flush(b64);

  BUF_MEM* bptr;
  BIO_get_mem_ptr(b64, &bptr);

  std::string signature(bptr->data, bptr->length);
  BIO_free_all(b64);

  return signature;
}

} // namespace pg_llm
