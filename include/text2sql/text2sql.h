#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>

#include "models/llm_interface.h"
#include "text2sql/pg_vector.h"

namespace pg_llm {
namespace text2sql {

// Database table structure information
struct TableInfo {
  std::string name;
  std::vector<std::pair<std::string, std::string>> columns; // <column_name, data_type>
  std::string description;
};

struct VectorSchemaInfo {
  std::string table_name;
  std::string column_name;
  int64_t row_id;
  float similarity;
  std::string metadata;
};

// Query analyzer
struct QueryAnalyzer {
  bool has_aggregation;
  bool has_joins;
  bool has_subqueries;
  bool has_window_functions;
  std::vector<std::string> suggested_indexes;
  std::vector<std::string> performance_tips;
};

// SQL type detection
enum class SQLType {
  SELECT,
  INSERT,
  UPDATE,
  DELETE,
  CREATE,
  ALTER,
  DROP,
  TRUNCATE,
  BEGIN,
  COMMIT,
  ROLLBACK,
  UNKNOWN
};

// Cache entry with timestamp
template<typename T>
struct CacheEntry {
  T data;
  std::chrono::system_clock::time_point timestamp;
};

// Text2SQL configuration
struct Text2SQLConfig {
  // Basic settings
  bool use_vector_search = true;
  int max_tables = 5;  // Maximum number of tables to return
  float similarity_threshold = 0.7;
  int max_tokens = 4000;  // Maximum token limit
  bool include_sample_data = true;  // Whether to include sample data
  int sample_data_limit = 5;  // Sample data row limit
  
  // Performance settings
  bool enable_cache = true;  // Enable caching
  int cache_ttl_seconds = 3600;  // Cache time-to-live in seconds
  int max_cache_size = 1000;  // Maximum number of cache entries
  int batch_size = 100;  // Batch size for vector operations
  bool parallel_processing = true;  // Enable parallel processing
  int max_parallel_threads = 4;  // Maximum number of parallel threads
};

// Text2SQL core class
class Text2SQL {
public:
  Text2SQL(std::shared_ptr<LLMInterface> model, const Text2SQLConfig& config = Text2SQLConfig());
  
  // Get database schema information
  std::vector<TableInfo> get_schema_info();
  
  // Get table sample data
  std::string get_table_sample_data(const std::string& table_name);
  
  // Vector search
  std::vector<VectorSchemaInfo> search_vectors(const std::string& query);
  std::vector<std::string> get_similar_queries(const std::string& query);
  
  // Generate SQL
  std::string generate_sql(const std::string& query, 
                         const std::vector<TableInfo>& schema,
                         const std::vector<VectorSchemaInfo>& search_results = {},
                         const std::vector<std::string>& similar_results = {});

  QueryAnalyzer analyze_query(const std::string& sql);

  SQLType detect_sql_type(const std::string& sql);

  std::string generate_query_suggestions(const std::string& sql);

  std::string validate_and_optimize_sql(const std::string& sql);

private:
  // Build prompt
  std::string build_prompt(const std::string& query,
                         const std::vector<TableInfo>& schema,
                         const std::vector<VectorSchemaInfo>& search_results,
                         const std::vector<std::string>& similar_results);
  
  std::string execute_and_format_sql(const std::string& sql);
  
  // Extract SQL from response
  std::string extract_sql(const std::string& response);

  // Cache management
  template<typename T>
  bool get_from_cache(const std::string& key, T& value);
  
  template<typename T>
  void set_cache(const std::string& key, const T& value);
  
  void cleanup_cache();
  
  // Parallel processing
  std::vector<VectorSchemaInfo> parallel_search(const std::string& query);
  
  // Batch processing
  std::vector<VectorSchemaInfo> batch_search(const std::vector<float>& embedding);

  std::shared_ptr<LLMInterface> model_;
  Text2SQLConfig config_;
  
  // Cache storage
  std::unordered_map<std::string, CacheEntry<std::vector<TableInfo>>> schema_cache_;
  std::unordered_map<std::string, CacheEntry<std::string>> sample_data_cache_;
  std::unordered_map<std::string, CacheEntry<std::vector<VectorSchemaInfo>>> vector_cache_;
  std::unordered_map<std::string, CacheEntry<std::string>> sql_cache_;
  
  // Mutex for thread safety
  std::mutex schema_cache_mutex_;
  std::mutex sample_data_cache_mutex_;
  std::mutex vector_cache_mutex_;
  std::mutex sql_cache_mutex_;
};

} // namespace text2sql
} // namespace pg_llm 
