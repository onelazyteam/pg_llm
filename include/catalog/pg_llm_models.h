#pragma once

extern "C" {
#include "postgres.h"
}

#include <string>
#include <vector>

struct PgLlmModelInfo {
  bool local_model = false;
  std::string model_type;
  std::string instance_name;
  std::string api_key;
  std::string config;
  std::string encrypted_api_key;
  std::string encrypted_config;
  double confidence_threshold = 0.0;
  std::string fallback_instance;
  bool is_local_fallback = false;
  std::string capabilities_json = "{}";
};

void pg_llm_model_insert(const PgLlmModelInfo& info);
bool pg_llm_model_delete(const std::string& instance_name);
bool pg_llm_model_get_info(const std::string& instance_name, PgLlmModelInfo* info);

bool pg_llm_model_get_infos(bool* local_model,
                            const std::string instance_name,
                            std::string& model_type,
                            std::string& api_key,
                            std::string& config);

std::vector<std::string> pg_llm_get_all_instancenames();
