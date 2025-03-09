#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "models/llm_interface.h"

#include <map>
#include <mutex>

namespace pg_llm {

class ModelManager {
public:
    static ModelManager& get_instance();

    // Register a new model type
    void register_model(const std::string& model_type, ModelCreator creator);

    // Create and initialize a new model instance
    bool create_model_instance(const std::string& model_type, 
                             const std::string& instance_name,
                             const std::string& api_key,
                             const std::string& model_config);

    // Remove a model instance
    bool remove_model_instance(const std::string& instance_name);

    // Get a model instance
    std::shared_ptr<LLMInterface> get_model(const std::string& instance_name);

    // Parallel inference with multiple models
    std::vector<ModelResponse> parallel_inference(const std::string& prompt,
                                                const std::vector<std::string>& model_names);

    // Multi-turn parallel inference
    std::vector<ModelResponse> parallel_inference(const std::vector<ChatMessage>& messages,
                                                const std::vector<std::string>& model_names);

    // Get best response based on confidence score
    ModelResponse get_best_response(const std::vector<ModelResponse>& responses);

private:
    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    std::map<std::string, ModelCreator> model_creators_;
    std::map<std::string, std::shared_ptr<LLMInterface>> model_instances_;
    std::mutex mutex_;
};

} // namespace pg_llm

#endif // MODEL_MANAGER_H 