#pragma once

#include "models/llm_interface.h"

namespace pg_llm {

class DeepSeekModel : public LLMInterface {
public:
  DeepSeekModel();
  ~DeepSeekModel() override;

  bool initialize(const std::string& api_key, const std::string& model_config) override;
  ModelResponse chat_completion(const std::string& prompt) override;
  ModelResponse chat_completion(const std::vector<ChatMessage>& messages) override;
  std::string get_model_name() const override;
  std::string get_model_info() const override;
  bool is_ready() const override;
  CURLcode make_api_request(const std::string& endpoint,
                            const std::string& request_body,
                            ResponseData &response_data) override;
  std::vector<float> get_embedding(const std::string& text) override;
};

} // namespace pg_llm
