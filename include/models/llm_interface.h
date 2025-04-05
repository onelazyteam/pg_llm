#pragma once

#include <cstring>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <json/json.h>
#include <openssl/buffer.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/types.h>

#include "utils/pg_llm_glog.h"

namespace pg_llm {

// Structure to hold chat message
struct ChatMessage {
  std::string role;
  std::string content;
};

// Structure to hold model response with confidence score
struct ModelResponse {
  std::string response;
  float confidence_score;
  std::string model_name;
};

// Response data accumulation structure
struct ResponseData {
  std::string content;    // Accumulated response content
  std::string fullReply;  // Final parsed reply content
};

// Custom structure to store streaming results and buffer
struct StreamContext {
  std::string buffer;      // Unprocessed data buffer
  std::string fullReply;   // Final concatenated response
};

// Abstract base class for LLM models
class LLMInterface {
public:
  LLMInterface(const std::string& model_type) :
               curl_(nullptr),
               is_initialized_(false),
               is_streaming_(false) {
    curl_ = curl_easy_init();
    if (!curl_) {
      PG_LLM_LOG_ERROR("Failed to initialize curl");
    }
    model_type_ = model_type;
  }

  virtual ~LLMInterface() {
      if (curl_) {
          curl_easy_cleanup(curl_);
      }
  }

  // Initialize the model with API key and other configurations
  bool initialize(bool local_model,
    const std::string& api_key,
    const std::string& model_config);

  // Single round chat completion
  ModelResponse chat_completion(const std::string& prompt);

  // Multi-turn chat completion
  ModelResponse chat_completion(const std::vector<ChatMessage>& messages);

  // Get model name
  std::string get_model_name() const;

  // Get model capabilities and parameters
  std::string get_model_info() const;

  // Validate if the model is ready for inference
  bool is_ready() const;

  CURLcode make_api_request(const std::string& endpoint,
                            const std::string& request_body,
                            ResponseData &response_data);

  // Get text embedding
  std::vector<float> get_embedding(const std::string& text);

  inline bool is_streaming() { return is_streaming_; }

protected:
  // Simplified callback function (direct data accumulation)
  static inline size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    ResponseData* resp = static_cast<ResponseData*>(userp);
    resp->content.append(static_cast<char*>(contents), realsize);
    return realsize;
  }

  size_t stream_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
  
  std::string generate_signature(const std::string& request_body);

private:
  CURL* curl_;
  std::string model_type_;
  std::string api_key_;
  std::string access_key_id_;
  std::string access_key_secret_;
  std::string model_name_;
  std::string api_endpoint_;
  bool local_model_;
  bool is_initialized_;
  bool is_streaming_;
};

// Factory function type for creating model instances
using ModelCreator = std::function<std::unique_ptr<LLMInterface>()>;
} // namespace pg_llm
