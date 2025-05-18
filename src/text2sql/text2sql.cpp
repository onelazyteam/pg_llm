#include "text2sql/text2sql.h"

extern "C" {
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonb.h"
#include "utils/lsyscache.h"
#include "utils/pg_llm_glog.h"
}

#include <thread>
#include <future>

namespace pg_llm {
namespace text2sql {

Text2SQL::Text2SQL(std::shared_ptr<LLMInterface> model, const Text2SQLConfig& config)
  : model_(model), config_(config) {}

// Cache management implementation
template<typename T>
bool Text2SQL::get_from_cache(const std::string& key, T& value) {
  if (!config_.enable_cache) return false;

  auto now = std::chrono::system_clock::now();
  std::unordered_map<std::string, CacheEntry<T>>* cache;
  std::mutex* mutex;

  if constexpr (std::is_same_v<T, std::vector<TableInfo>>) {
    cache = &schema_cache_;
    mutex = &schema_cache_mutex_;
  } else if constexpr (std::is_same_v<T, std::string>) {
    cache = &sample_data_cache_;
    mutex = &sample_data_cache_mutex_;
  } else if constexpr (std::is_same_v<T, std::vector<VectorSchemaInfo>>) {
    cache = &vector_cache_;
    mutex = &vector_cache_mutex_;
  } else {
    cache = &sql_cache_;
    mutex = &sql_cache_mutex_;
  }

  std::lock_guard<std::mutex> lock(*mutex);
  auto it = cache->find(key);
  if (it != cache->end()) {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                 now - it->second.timestamp).count();
    if (age < config_.cache_ttl_seconds) {
      value = it->second.data;
      return true;
    }
    cache->erase(it);
  }
  return false;
}

template<typename T>
void Text2SQL::set_cache(const std::string& key, const T& value) {
  if (!config_.enable_cache) return;

  std::unordered_map<std::string, CacheEntry<T>>* cache;
  std::mutex* mutex;

  if constexpr (std::is_same_v<T, std::vector<TableInfo>>) {
    cache = &schema_cache_;
    mutex = &schema_cache_mutex_;
  } else if constexpr (std::is_same_v<T, std::string>) {
    cache = &sample_data_cache_;
    mutex = &sample_data_cache_mutex_;
  } else if constexpr (std::is_same_v<T, std::vector<VectorSchemaInfo>>) {
    cache = &vector_cache_;
    mutex = &vector_cache_mutex_;
  } else {
    cache = &sql_cache_;
    mutex = &sql_cache_mutex_;
  }

  std::lock_guard<std::mutex> lock(*mutex);
  if (cache->size() >= static_cast<size_t>(config_.max_cache_size)) {
    cleanup_cache();
  }

  CacheEntry<T> entry{value, std::chrono::system_clock::now()};
  (*cache)[key] = entry;
}

void Text2SQL::cleanup_cache() {
  auto now = std::chrono::system_clock::now();

  auto cleanup = [&](auto& cache, auto& mutex) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = cache.begin(); it != cache.end();) {
      auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   now - it->second.timestamp).count();
      if (age >= config_.cache_ttl_seconds) {
        it = cache.erase(it);
      } else {
        ++it;
      }
    }
  };

  cleanup(schema_cache_, schema_cache_mutex_);
  cleanup(sample_data_cache_, sample_data_cache_mutex_);
  cleanup(vector_cache_, vector_cache_mutex_);
  cleanup(sql_cache_, sql_cache_mutex_);
}

// Parallel search implementation
std::vector<VectorSchemaInfo> Text2SQL::parallel_search(const std::string& query) {
  if (!config_.parallel_processing) {
    return search_vectors(query);
  }

  auto embedding = model_->get_embedding(query);
  return batch_search(embedding);
}

std::vector<VectorSchemaInfo> Text2SQL::batch_search(const std::vector<float>& embedding) {
  std::vector<VectorSchemaInfo> results;
  Datum embedding_datum = std_vector_to_vector(embedding);

  SPI_connect();
  char sql[1024];
  snprintf(sql,
           sizeof(sql),
           "SELECT table_name, column_name, row_id, "
           "1 - (vector <=> $1) as similarity, metadata "
           "FROM _pg_llm_catalog.pg_llm_vectors "
           "WHERE 1 - (vector <=> $1) >= $2 "
           "ORDER BY vector <=> $1 "
           "LIMIT $3");

  Oid argtypes[3] = {get_vector_type_oid(), FLOAT4OID, INT4OID};
  Datum values[3] = {embedding_datum,
                     Float4GetDatum(config_.similarity_threshold),
                     Int32GetDatum(config_.max_tables)
                    };
  char nulls[3] = {' ', ' ', ' '};

  SPIPlanPtr plan = SPI_prepare(sql, 3, argtypes);
  if (plan == NULL) {
    SPI_finish();
    return {};
  }

  int ret = SPI_execute_plan(plan, values, nulls, true, 1);
  if (ret != SPI_OK_SELECT) {
    elog(ERROR, "SPI_execute_plan failed: %s", SPI_result_code_string(ret));
  }
  SPITupleTable* tuptable = SPI_tuptable;
  if (tuptable == NULL) {
    SPI_finish();
    return {};
  }

  for (uint64 i = 0; i < SPI_processed; i++) {
    VectorSchemaInfo result;
    bool isnull;
    result.table_name = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 1);
    result.column_name = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 2);
    result.row_id = DatumGetInt64(SPI_getbinval(tuptable->vals[i], tuptable->tupdesc, 3, &isnull));
    result.similarity = DatumGetFloat4(SPI_getbinval(tuptable->vals[i], tuptable->tupdesc, 4, &isnull));
    result.metadata = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 5);
    results.push_back(result);
  }

  SPI_finish();
  return results;
}

// Optimized schema info retrieval
std::vector<TableInfo> Text2SQL::get_schema_info() {
  std::vector<TableInfo> schema;
  std::string cache_key = "schema_info";

  if (get_from_cache(cache_key, schema)) {
    return schema;
  }

  SPI_connect();
  const char* query = "SELECT table_name, column_name, data_type, "
                      "obj_description(('public.' || table_name)::regclass) as description "
                      "FROM information_schema.columns "
                      "WHERE table_schema = 'public' "
                      "ORDER BY table_name, ordinal_position";

  int ret = SPI_execute(query, true, 0);
  if (ret != SPI_OK_SELECT) {
    SPI_finish();
    ereport(ERROR,
            (errcode(ERRCODE_INTERNAL_ERROR),
             errmsg("Failed to get schema information")));
  }

  std::string current_table;
  TableInfo* current_table_info = nullptr;

  for (uint64 i = 0; i < SPI_processed; i++) {
    char* table_name = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1);
    char* column_name = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2);
    char* data_type = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 3);
    char* description = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 4);

    if (current_table != table_name) {
      if (current_table_info) {
        schema.push_back(*current_table_info);
        delete current_table_info;
      }
      current_table = table_name;
      current_table_info = new TableInfo();
      current_table_info->name = table_name;
      current_table_info->description = description ? description : "";
    }

    current_table_info->columns.emplace_back(column_name, data_type);
  }

  if (current_table_info) {
    schema.push_back(*current_table_info);
    delete current_table_info;
  }

  SPI_finish();

  set_cache(cache_key, schema);
  return schema;
}

std::string Text2SQL::get_table_sample_data(const std::string& table_name) {
  if (!config_.include_sample_data) {
    return "";
  }

  SPI_connect();
  char sql[1024];
  snprintf(sql,
           sizeof(sql),
           "SELECT * FROM %s LIMIT %d",
           table_name.c_str(),
           config_.sample_data_limit);

  int ret = SPI_execute(sql, true, 0);
  if (ret != SPI_OK_SELECT) {
    SPI_finish();
    return "";
  }

  std::string result = "Sample data:\n";
  for (uint64 i = 0; i < SPI_processed; i++) {
    result += "Row " + std::to_string(i + 1) + ": ";
    for (int j = 0; j < SPI_tuptable->tupdesc->natts; j++) {
      char* value = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, j + 1);
      result += std::string(SPI_tuptable->tupdesc->attrs[j].attname.data) +
                "=" + (value ? value : "NULL") + " ";
    }
    result += "\n";
  }

  SPI_finish();
  return result;
}

std::vector<std::string> Text2SQL::get_similar_queries(const std::string& query) {
  if (!config_.use_vector_search) {
    return {};
  }

  auto embedding = model_->get_embedding(query);
  Datum embedding_datum = std_vector_to_vector(embedding);

  SPI_connect();
  char sql[1024];
  snprintf(sql,
           sizeof(sql),
           "SELECT nl_sql_pair FROM _pg_llm_catalog.pg_llm_queries "
           "WHERE 1 - (question <=> $1) >= $2 "
           "ORDER BY question <=> $1 "
           "LIMIT $3");

  Oid argtypes[3] = {get_vector_type_oid(), FLOAT4OID, INT4OID};
  Datum values[3] = {embedding_datum,
                     Float4GetDatum(config_.similarity_threshold),
                     Int32GetDatum(config_.sample_data_limit)
                    };
  char nulls[3] = {' ', ' ', ' '};

  SPIPlanPtr plan = SPI_prepare(sql, 3, argtypes);
  if (plan == NULL) {
    SPI_finish();
    return {};
  }

  int ret = SPI_execute_plan(plan, values, nulls, true, 1);
  if (ret != SPI_OK_SELECT) {
    elog(ERROR, "SPI_execute_plan failed: %s", SPI_result_code_string(ret));
  }
  SPITupleTable* tuptable = SPI_tuptable;
  if (tuptable == NULL) {
    SPI_finish();
    return {};
  }

  std::vector<std::string> results;
  for (uint64 i = 0; i < SPI_processed; i++) {
    char* query_str = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 1);
    results.push_back(query_str ? query_str : "");
  }

  SPI_finish();
  return results;
}

std::vector<VectorSchemaInfo> Text2SQL::search_vectors(const std::string& query) {
  if (!config_.use_vector_search) {
    return {};
  }

  auto embedding = model_->get_embedding(query);
  Datum embedding_datum = std_vector_to_vector(embedding);

  SPI_connect();
  char sql[1024];
  snprintf(sql,
           sizeof(sql),
           "SELECT table_name, column_name, row_id, "
           "1 - (query_vector <=> $1) as similarity, metadata "
           "FROM _pg_llm_catalog.pg_llm_vectors "
           "WHERE 1 - (query_vector <=> $1) >= $2 "
           "ORDER BY query_vector <=> $1 "
           "LIMIT $3");

  Oid argtypes[3] = {get_vector_type_oid(), FLOAT4OID, INT4OID};
  Datum values[3] = {embedding_datum,
                     Float4GetDatum(config_.similarity_threshold),
                     Int32GetDatum(config_.max_tables)
                    };
  char nulls[3] = {' ', ' ', ' '};

  SPIPlanPtr plan = SPI_prepare(sql, 3, argtypes);
  if (plan == NULL) {
    SPI_finish();
    return {};
  }

  int ret = SPI_execute_plan(plan, values, nulls, true, 1);
  if (ret != SPI_OK_SELECT) {
    elog(ERROR, "SPI_execute_plan failed: %s", SPI_result_code_string(ret));
  }
  SPITupleTable* tuptable = SPI_tuptable;
  if (tuptable == NULL) {
    SPI_finish();
    return {};
  }

  std::vector<VectorSchemaInfo> results;
  for (uint64 i = 0; i < SPI_processed; i++) {
    VectorSchemaInfo result;
    bool isnull;
    result.table_name = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 1);
    result.column_name = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 2);
    result.row_id = DatumGetInt64(SPI_getbinval(tuptable->vals[i], tuptable->tupdesc, 3, &isnull));
    result.similarity = DatumGetFloat4(SPI_getbinval(tuptable->vals[i], tuptable->tupdesc, 4, &isnull));
    result.metadata = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 5);
    results.push_back(result);
  }

  SPI_finish();
  return results;
}

QueryAnalyzer Text2SQL::analyze_query(const std::string& sql) {
    QueryAnalyzer analyzer;
    analyzer.has_aggregation = sql.find("GROUP BY") != std::string::npos || 
                             sql.find("HAVING") != std::string::npos;
    analyzer.has_joins = sql.find("JOIN") != std::string::npos;
    analyzer.has_subqueries = sql.find("(SELECT") != std::string::npos;
    analyzer.has_window_functions = sql.find("OVER (") != std::string::npos;
    
    // Analyze potential index requirements
    if (sql.find("WHERE") != std::string::npos) {
        size_t where_pos = sql.find("WHERE");
        std::string where_clause = sql.substr(where_pos);
        if (where_clause.find("LIKE") != std::string::npos) {
            analyzer.suggested_indexes.push_back("Consider creating an index for LIKE query columns");
        }
    }
    
    // Performance tips
    if (analyzer.has_joins) {
        analyzer.performance_tips.push_back("Ensure proper indexes on JOIN condition columns");
    }
    if (analyzer.has_aggregation) {
        analyzer.performance_tips.push_back("Consider using materialized views for aggregation queries");
    }
    
    return analyzer;
}

SQLType Text2SQL::detect_sql_type(const std::string& sql) {
    std::string upper_sql = sql;
    std::transform(upper_sql.begin(), upper_sql.end(), upper_sql.begin(), ::toupper);
    
    if (upper_sql.find("SELECT") == 0) return SQLType::SELECT;
    if (upper_sql.find("INSERT") == 0) return SQLType::INSERT;
    if (upper_sql.find("UPDATE") == 0) return SQLType::UPDATE;
    if (upper_sql.find("DELETE") == 0) return SQLType::DELETE;
    if (upper_sql.find("CREATE") == 0) return SQLType::CREATE;
    if (upper_sql.find("ALTER") == 0) return SQLType::ALTER;
    if (upper_sql.find("DROP") == 0) return SQLType::DROP;
    if (upper_sql.find("TRUNCATE") == 0) return SQLType::TRUNCATE;
    if (upper_sql.find("BEGIN") == 0) return SQLType::BEGIN;
    if (upper_sql.find("COMMIT") == 0) return SQLType::COMMIT;
    if (upper_sql.find("ROLLBACK") == 0) return SQLType::ROLLBACK;
    
    return SQLType::UNKNOWN;
}

// Query optimization suggestion generator
std::string Text2SQL::generate_query_suggestions(const std::string& sql) {
    QueryAnalyzer analyzer = analyze_query(sql);
    std::string suggestions = "\nQuery Analysis and Suggestions:\n";
    
    if (!analyzer.suggested_indexes.empty()) {
        suggestions += "\nIndex Suggestions:\n";
        for (const auto& index : analyzer.suggested_indexes) {
            suggestions += "- " + index + "\n";
        }
    }
    
    if (!analyzer.performance_tips.empty()) {
        suggestions += "\nPerformance Optimization Tips:\n";
        for (const auto& tip : analyzer.performance_tips) {
            suggestions += "- " + tip + "\n";
        }
    }
    
    // Add query complexity analysis
    suggestions += "\nQuery Complexity Analysis:\n";
    if (analyzer.has_aggregation) suggestions += "- Contains aggregation operations\n";
    if (analyzer.has_joins) suggestions += "- Contains table joins\n";
    if (analyzer.has_subqueries) suggestions += "- Contains subqueries\n";
    if (analyzer.has_window_functions) suggestions += "- Contains window functions\n";
    
    return suggestions;
}

// Modify build_prompt function to support more SQL types
std::string Text2SQL::build_prompt(const std::string& query,
                                   const std::vector<TableInfo>& schema,
                                   const std::vector<VectorSchemaInfo>& search_results,
                                   const std::vector<std::string>& similar_results) {
    std::string prompt = "You are a professional PostgreSQL database engineer. Your task is to convert natural language queries into accurate SQL statements.\n\n"
                       "Important Rules:\n"
                       "1. Generate SQL in a SINGLE LINE, no line breaks\n"
                       "2. Use ONLY columns that exist in the table\n"
                       "3. Do NOT add IS NOT NULL conditions unless explicitly requested\n"
                       "4. Do NOT add any line breaks or indentation\n"
                       "5. Use proper PostgreSQL syntax and functions\n"
                       "6. Include GROUP BY clauses only when using aggregate functions\n"
                       "7. Include ORDER BY clauses only when sorting is requested\n"
                       "8. Use proper data type casting when needed\n"
                       "9. Include LIMIT clauses only when explicitly requested\n"
                       "10. Use subqueries only when necessary\n"
                       "11. Handle date/time operations correctly\n"
                       "12. Use proper string operations and pattern matching\n"
                       "13. Consider query performance implications\n"
                       "14. SQL statements MUST end with a semicolon (;)\n"
                       "15. Support DDL statements (CREATE, ALTER, DROP, etc.)\n"
                       "16. Support transaction control (BEGIN, COMMIT, ROLLBACK)\n\n";

    // Add database schema information
    prompt += "DATABASE SCHEMA:\n";
    for (const auto& table : schema) {
        prompt += "Table: " + table.name + "\n";
        if (!table.description.empty()) {
            prompt += "Description: " + table.description + "\n";
        }
        prompt += "Columns (ONLY use these columns in your query):\n";
        for (const auto& column : table.columns) {
            prompt += "  " + column.first + " (" + column.second + ")\n";
        }
        
        // Add sample data for better context
        std::string sample_data = get_table_sample_data(table.name);
        if (!sample_data.empty()) {
            prompt += "Sample Data:\n" + sample_data + "\n";
        }
    }

    // Add vector search schema results for context
    if (!search_results.empty()) {
        prompt += "\nRELEVANT DATA CONTEXT:\n";
        for (const auto& result : search_results) {
            prompt += "Table: " + result.table_name + "\n";
            prompt += "Column: " + result.column_name + "\n";
            prompt += "Row ID: " + std::to_string(result.row_id) + "\n";
            prompt += "Similarity Score: " + std::to_string(result.similarity) + "\n";
            if (!result.metadata.empty()) {
                prompt += "Metadata: " + result.metadata + "\n";
            }
            prompt += "---\n";
        }
    }

    // Add similar queries for context
    if (!similar_results.empty()) {
        prompt += "\nSIMILAR QUERIES:\n";
        for (const auto& similar_query : similar_results) {
            prompt += "- " + similar_query + "\n";
        }
        prompt += "\n";
    }

    // Add query context and requirements
    prompt += "\nQUERY REQUIREMENTS:\n";
    prompt += "1. Current Natural Language Query: " + query + "\n";
    prompt += "2. Required Output Format: PostgreSQL SQL query in a SINGLE LINE ending with semicolon\n";
    prompt += "3. Need to refer to schema information and similar queries information";
    return prompt;
}

std::string Text2SQL::extract_sql(const std::string& response) {
  // Find the start position of SQL statement
  size_t start = response.find("SELECT");
  if (start == std::string::npos) {
    start = response.find("select");
  }
  if (start == std::string::npos) {
    start = response.find("INSERT");
    if (start == std::string::npos) {
      start = response.find("insert");
    }
  }
  if (start == std::string::npos) {
    start = response.find("UPDATE");
    if (start == std::string::npos) {
      start = response.find("update");
    }
  }
  if (start == std::string::npos) {
    start = response.find("DELETE");
    if (start == std::string::npos) {
      start = response.find("delete");
    }
  }
  if (start == std::string::npos) {
    return response;
  }

  // Extract from start position to the end of string
  std::string sql = response.substr(start);
  
  // Clean up possible extra content
  size_t end = sql.find("```");
  if (end != std::string::npos) {
    sql = sql.substr(0, end);
  }
  
  // Remove trailing whitespace characters
  while (!sql.empty() && (sql.back() == ' ' || sql.back() == '\n' || sql.back() == '\r')) {
    sql.pop_back();
  }
  
  return sql;
}

// Add a new helper function for formatting query results
static inline std::string format_query_results(SPITupleTable* result_tuptable) {
    std::string result;
    if (result_tuptable != NULL) {
        // Add column headers
        std::vector<std::string> headers;
        std::vector<size_t> col_widths;
        for (int j = 0; j < result_tuptable->tupdesc->natts; j++) {
            headers.push_back(result_tuptable->tupdesc->attrs[j].attname.data);
            col_widths.push_back(headers[j].length());
        }

        // Calculate column widths
        for (uint64 i = 0; i < SPI_processed; i++) {
            for (int j = 0; j < result_tuptable->tupdesc->natts; j++) {
                char* value = SPI_getvalue(result_tuptable->vals[i], result_tuptable->tupdesc, j + 1);
                if (value) {
                    col_widths[j] = std::max(col_widths[j], strlen(value));
                }
            }
        }

        // Print headers
        result += " ";
        for (int j = 0; j < result_tuptable->tupdesc->natts; j++) {
            result += headers[j];
            result += std::string(col_widths[j] - headers[j].length() + 2, ' ');
        }
        result += "\n";

        // Print separator
        result += "-";
        for (int j = 0; j < result_tuptable->tupdesc->natts; j++) {
            result += std::string(col_widths[j] + 2, '-');
        }
        result += "\n";

        // Print rows
        for (uint64 i = 0; i < SPI_processed; i++) {
            result += " ";
            for (int j = 0; j < result_tuptable->tupdesc->natts; j++) {
                char* value = SPI_getvalue(result_tuptable->vals[i], result_tuptable->tupdesc, j + 1);
                if (value) {
                    result += value;
                    result += std::string(col_widths[j] - strlen(value) + 2, ' ');
                } else {
                    result += "NULL";
                    result += std::string(col_widths[j] - 4 + 2, ' ');
                }
            }
            result += "\n";
        }

        // Print row count
        result += "(" + std::to_string(SPI_processed) + " rows)\n";
    }
    return result;
}

// Modify execute_and_format_sql to use the new helper function
std::string Text2SQL::execute_and_format_sql(const std::string& sql) {
    if (sql.empty()) {
        return "Error: Empty SQL statement";
    }

    std::string result = "Generated SQL: " + sql + "\n\n";
    
    SQLType sql_type = detect_sql_type(sql);
    
    SPI_connect();
    
    // Execute SQL query
    int ret = SPI_execute(sql.c_str(), false, 0);
    if (ret != SPI_OK_SELECT && ret != SPI_OK_INSERT && 
        ret != SPI_OK_UPDATE && ret != SPI_OK_DELETE && 
        ret != SPI_OK_UTILITY) {
        SPI_finish();
        return result + "Error: Failed to execute SQL: " + std::string(SPI_result_code_string(ret));
    }

    // Handle results based on SQL type
    switch (sql_type) {
        case SQLType::SELECT:
            result += format_query_results(SPI_tuptable);
            break;
            
        case SQLType::INSERT:
        case SQLType::UPDATE:
        case SQLType::DELETE:
            result += std::to_string(SPI_processed) + " rows affected.\n";
            break;
            
        case SQLType::CREATE:
        case SQLType::ALTER:
        case SQLType::DROP:
            result += "DDL statement executed successfully.\n";
            break;
            
        case SQLType::BEGIN:
            result += "Transaction started.\n";
            break;
            
        case SQLType::COMMIT:
            result += "Transaction committed.\n";
            break;
            
        case SQLType::ROLLBACK:
            result += "Transaction rolled back.\n";
            break;
            
        default:
            result += "Unknown SQL type.\n";
    }

    // Add query analysis and suggestions
    result += generate_query_suggestions(sql);

    SPI_finish();
    return result;
}

// Add new error handling class
class SQLError : public std::runtime_error {
public:
    enum class ErrorType {
        SYNTAX_ERROR,
        PERMISSION_ERROR,
        CONNECTION_ERROR,
        TIMEOUT_ERROR,
        VALIDATION_ERROR
    };

    SQLError(ErrorType type, const std::string& message)
        : std::runtime_error(message), type_(type) {}

    ErrorType type() const { return type_; }

private:
    ErrorType type_;
};

// Modify validate_and_optimize_sql to use the new helper function
std::string Text2SQL::validate_and_optimize_sql(const std::string& sql) {
    if (sql.empty()) {
        throw SQLError(SQLError::ErrorType::VALIDATION_ERROR, "Empty SQL statement");
    }

    // Only validate and optimize SELECT queries
    std::string upper_sql = sql;
    std::transform(upper_sql.begin(), upper_sql.end(), upper_sql.begin(), ::toupper);
    if (upper_sql.find("SELECT") != 0) {
        return sql;
    }

    SPI_connect();
    std::string result;

    // First execute the SELECT query to get results
    int ret = SPI_execute(sql.c_str(), false, 0);
    if (ret != SPI_OK_SELECT) {
        SPI_finish();
        throw SQLError(SQLError::ErrorType::SYNTAX_ERROR, 
                      "Invalid SQL: " + std::string(SPI_result_code_string(ret)));
    }

    // Format results using the helper function
    if (SPI_tuptable != NULL) {
        result += format_query_results(SPI_tuptable);
    }

    // Then check query plan for optimization suggestions
    std::string explain_sql = "EXPLAIN " + sql;
    ret = SPI_execute(explain_sql.c_str(), false, 0);
    if (ret == SPI_OK_UTILITY) {
        SPITupleTable* tuptable = SPI_tuptable;
        if (tuptable != NULL) {
            bool needs_limit = false;
            std::string table_name;
            std::string column_name;
            
            for (uint64 i = 0; i < SPI_processed; i++) {
                char* plan_line = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 1);
                if (plan_line) {
                    std::string line(plan_line);
                    if (line.find("Seq Scan") != std::string::npos) {
                        // Extract table name from Seq Scan line
                        size_t pos = line.find("on ");
                        if (pos != std::string::npos) {
                            size_t start = pos + 3;
                            size_t end = line.find(" ", start);
                            if (end != std::string::npos) {
                                table_name = line.substr(start, end - start);
                            }
                        }
                        
                        // Try to extract column name from WHERE clause
                        size_t where_pos = sql.find("WHERE");
                        if (where_pos != std::string::npos) {
                            std::string where_clause = sql.substr(where_pos);
                            size_t op_pos = where_clause.find("=");
                            if (op_pos != std::string::npos) {
                                size_t col_start = where_clause.find_last_of(" ", op_pos - 1);
                                if (col_start != std::string::npos) {
                                    column_name = where_clause.substr(col_start + 1, op_pos - col_start - 1);
                                }
                            }
                        }
                    }
                    if (line.find("cost=") != std::string::npos && 
                        line.find("rows=") != std::string::npos && 
                        line.find("rows=") > line.find("cost=")) {
                        size_t rows_start = line.find("rows=") + 5;
                        size_t rows_end = line.find(" ", rows_start);
                        if (rows_end != std::string::npos) {
                            int estimated_rows = std::stoi(line.substr(rows_start, rows_end - rows_start));
                            if (estimated_rows > 1000) {
                                needs_limit = true;
                            }
                        }
                    }
                }
            }

            // Add optimization suggestions at the beginning
            if (!table_name.empty() || needs_limit) {
                result = "-- Optimization suggestions:\n" + result;
                
                if (!table_name.empty() && !column_name.empty()) {
                    // Check if index already exists
                    std::string index_query = "SELECT COUNT(*) FROM pg_indexes WHERE tablename = '" + 
                                            table_name + "' AND indexdef LIKE '%" + column_name + "%'";
                    ret = SPI_execute(index_query.c_str(), true, 0);
                    if (ret == SPI_OK_SELECT && SPI_processed > 0) {
                        char* count_str = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
                        if (count_str && std::stoi(count_str) == 0) {
                            result = "-- Consider creating an index on column '" + column_name + 
                                    "' of table '" + table_name + "':\n" +
                                    "-- CREATE INDEX idx_" + table_name + "_" + column_name + 
                                    " ON " + table_name + " (" + column_name + ");\n" + result;
                        }
                    }
                }
                
                if (needs_limit) {
                    result = "-- Consider adding LIMIT clause for large result sets\n" + result;
                }
            }
        }
    }

    SPI_finish();
    return result;
}

// Remove retry mechanism and simplify SQL generation
std::string Text2SQL::generate_sql(const std::string& query,
                                   const std::vector<TableInfo>& schema,
                                   const std::vector<VectorSchemaInfo>& search_results,
                                   const std::vector<std::string>& similar_results) {
    try {
        // Build prompt and generate SQL
        std::string prompt = build_prompt(query, schema, search_results, similar_results);
        auto response = model_->chat_completion(prompt);
        std::string sql = extract_sql(response.response);
        
        // Validate and optimize the generated SQL
        // TODO(YH) sql = validate_and_optimize_sql(sql);

        // Execute and get results
        return execute_and_format_sql(sql);
    } catch (const SQLError& e) {
        return "Error: " + std::string(e.what());
    }
}

} // namespace text2sql
} // namespace pg_llm