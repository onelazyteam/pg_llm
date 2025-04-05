#include "models/model_manager.h"

#include <future>
#include <thread>

#include "catalog/pg_llm_models.h"
#include "models/llm_interface.h"

namespace pg_llm {

ModelManager& ModelManager::get_instance() {
  static ModelManager instance;
  return instance;
}

void ModelManager::register_model(const std::string& model_type, ModelCreator creator) {
  std::lock_guard<std::mutex> lock(mutex_);
  model_creators_[model_type] = creator;
}

bool ModelManager::create_model_instance(bool local_model,
    const std::string& model_type,
    const std::string& instance_name,
    const std::string& api_key,
    const std::string& model_config) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto creator_it = model_creators_.find(model_type);
  if (creator_it == model_creators_.end()) {
    return false;
  }

  auto model = creator_it->second();
  if (!model->initialize(local_model, api_key, model_config)) {
    PG_LLM_LOG_FATAL("model:%s init failed.", model_type.c_str());
    return false;
  }

  model_instances_[instance_name] = std::move(model);
  return true;
}

bool ModelManager::remove_model_instance(const std::string& instance_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  return model_instances_.erase(instance_name) > 0;
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
  bool local_model = false;
  bool result = pg_llm_model_get_infos(&local_model, instance_name, model_type, api_key, config);
  if (unlikely(!result)) {
    return nullptr;
  } else {
    auto& manager = pg_llm::ModelManager::get_instance();
    manager.register_model(model_type, [model_type]() {
      return std::make_unique<pg_llm::LLMInterface>(model_type);
    });
    result = manager.create_model_instance(local_model, model_type, instance_name, api_key, config);
    if (result) {
      return get_model(instance_name);
    } else {
      return nullptr;
    }
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
