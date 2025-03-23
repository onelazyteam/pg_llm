#include "models/model_manager.h"

#include <future>
#include <thread>

#include "catalog/pg_llm_models.h"
#include "models/chatgpt_model.h"
#include "models/deepseek_model.h"
#include "models/hunyuan_model.h"
#include "models/llm_interface.h"
#include "models/qianwen_model.h"

namespace pg_llm {

ModelManager& ModelManager::get_instance() {
  static ModelManager instance;
  return instance;
}

void ModelManager::register_model(const std::string& model_type, ModelCreator creator) {
  std::lock_guard<std::mutex> lock(mutex_);
  model_creators_[model_type] = creator;
}

bool ModelManager::create_model_instance(const std::string& model_type,
    const std::string& instance_name,
    const std::string& api_key,
    const std::string& model_config) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto creator_it = model_creators_.find(model_type);
  if (creator_it == model_creators_.end()) {
    return false;
  }

  auto model = creator_it->second();
  if (!model->initialize(api_key, model_config)) {
    return false;
  }

  model_instances_[instance_name] = std::move(model);
  return true;
}

bool ModelManager::remove_model_instance(const std::string& instance_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  return model_instances_.erase(instance_name) > 0;
}

void ModelManager::add_model_internal(const std::string model_type) {
  // Register available models
  auto& manager = pg_llm::ModelManager::get_instance();
  if (model_type == "chatgpt") {
    manager.register_model("chatgpt", []() {
      return std::make_unique<pg_llm::ChatGPTModel>();
    });
  } else if (model_type == "deepseek") {
    manager.register_model("deepseek", []() {
      return std::make_unique<pg_llm::DeepSeekModel>();
    });
  } else if (model_type == "hunyuan") {
    manager.register_model("hunyuan", []() {
      return std::make_unique<pg_llm::HunyuanModel>();
    });
  } else if (model_type == "qianwen") {
    manager.register_model("qianwen", []() {
      return std::make_unique<pg_llm::QianwenModel>();
    });
  } else {
    pg_llm_log_error(__FILE__, __LINE__, "unknown type of model.");
  }
}

std::shared_ptr<LLMInterface> ModelManager::get_model(const std::string& instance_name) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = model_instances_.find(instance_name);
    if (it != model_instances_.end()) {
      return it->second;
    }
  }

  // get from catalog table
  std::string model_type, api_key, config;
  bool result = pg_llm_model_get_infos(instance_name, model_type, api_key, config);
  if (unlikely(!result)) {
    return nullptr;
  } else {
    add_model_internal(model_type);
    auto& manager = pg_llm::ModelManager::get_instance();
    manager.create_model_instance(model_type, instance_name, api_key, config);
    return get_model(instance_name);
  }
}

std::vector<ModelResponse> ModelManager::parallel_inference(
  const std::string& prompt,
  const std::vector<std::string>& model_names) {

  std::vector<std::future<ModelResponse>> futures;
  std::vector<ModelResponse> responses;

  // Launch parallel inference tasks
  for (const auto& model_name : model_names) {
    auto model = get_model(model_name);
    if (!model) continue;

    futures.push_back(std::async(std::launch::async,
    [model, prompt]() {
      return model->chat_completion(prompt);
    }
                                ));
  }

  // Collect results
  for (auto& future : futures) {
    responses.push_back(future.get());
  }

  return responses;
}

std::vector<ModelResponse> ModelManager::parallel_inference(
  const std::vector<ChatMessage>& messages,
  const std::vector<std::string>& model_names) {

  std::vector<std::future<ModelResponse>> futures;
  std::vector<ModelResponse> responses;

  // Launch parallel inference tasks
  for (const auto& model_name : model_names) {
    auto model = get_model(model_name);
    if (!model) continue;

    futures.push_back(std::async(std::launch::async,
    [model, messages]() {
      return model->chat_completion(messages);
    }
                                ));
  }

  // Collect results
  for (auto& future : futures) {
    responses.push_back(future.get());
  }

  return responses;
}

ModelResponse ModelManager::get_best_response(
  const std::vector<ModelResponse>& responses) {
  if (responses.empty()) {
    return ModelResponse{"No response available", 0.0f, "none"};
  }

  auto best_response = responses[0];
  for (const auto& response : responses) {
    if (response.confidence_score > best_response.confidence_score) {
      best_response = response;
    }
  }

  return best_response;
}

} // namespace pg_llm
