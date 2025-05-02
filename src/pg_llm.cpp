extern "C" {
#include "postgres.h"  // clang-format off
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonb.h"
#include "utils/lsyscache.h"
#include "utils/timestamp.h"
#include "utils/uuid.h"

#ifdef PG_MODULE_MAGIC
  PG_MODULE_MAGIC;
#endif

  /* Declare PostgreSQL extension functions */
  PG_FUNCTION_INFO_V1(pg_llm_add_model);
  PG_FUNCTION_INFO_V1(pg_llm_remove_model);
  PG_FUNCTION_INFO_V1(pg_llm_chat);
  PG_FUNCTION_INFO_V1(pg_llm_parallel_chat);
  PG_FUNCTION_INFO_V1(pg_llm_text2sql);
  PG_FUNCTION_INFO_V1(pg_llm_store_vector);
  PG_FUNCTION_INFO_V1(pg_llm_search_vectors);
  PG_FUNCTION_INFO_V1(pg_llm_get_embedding);
  PG_FUNCTION_INFO_V1(pg_llm_multi_turn_chat);
  PG_FUNCTION_INFO_V1(pg_llm_create_session);
  PG_FUNCTION_INFO_V1(pg_llm_cleanup_sessions);
  PG_FUNCTION_INFO_V1(pg_llm_get_sessions);
  PG_FUNCTION_INFO_V1(pg_llm_set_max_messages);

  Datum pg_llm_add_model(PG_FUNCTION_ARGS);
  Datum pg_llm_remove_model(PG_FUNCTION_ARGS);
  Datum pg_llm_chat(PG_FUNCTION_ARGS);
  Datum pg_llm_parallel_chat(PG_FUNCTION_ARGS);
  Datum pg_llm_text2sql(PG_FUNCTION_ARGS);
  Datum pg_llm_store_vector(PG_FUNCTION_ARGS);
  Datum pg_llm_search_vectors(PG_FUNCTION_ARGS);
  Datum pg_llm_get_embedding(PG_FUNCTION_ARGS);
  Datum pg_llm_multi_turn_chat(PG_FUNCTION_ARGS);
  Datum pg_llm_create_session(PG_FUNCTION_ARGS);
  Datum pg_llm_cleanup_sessions(PG_FUNCTION_ARGS);
  Datum pg_llm_set_max_messages(PG_FUNCTION_ARGS);
  Datum pg_llm_get_sessions(PG_FUNCTION_ARGS);

  /* Extension initialization and finalization functions */
  void _PG_init(void);
  void _PG_fini(void);
}  // extern "C"

// Include our wrapper header
#include "catalog/pg_llm_models.h"

#include "models/llm_interface.h"
#include "models/model_manager.h"
#include "models/session_manager.h"
#include "text2sql/text2sql.h"
#include "utils/pg_llm_glog.h"

/*
 * _PG_init
 *
 * Extension initialization function.
 * This function is called when the extension is loaded.
 */
void _PG_init(void) {
  /* Initialize GUC parameters for glog */
  pg_llm_glog_init_guc();

  /* Initialize glog */
  pg_llm_glog_init();

  PG_LLM_LOG_INFO("pg_llm extension loaded");
}

/*
 * _PG_fini
 *
 * Extension finalization function.
 * This function is called when the extension is unloaded.
 */
void _PG_fini(void) {
  PG_LLM_LOG_INFO("pg_llm extension unloaded");

  /* Shutdown glog */
  pg_llm_glog_shutdown();
}

// Add a new model instance
Datum pg_llm_add_model(PG_FUNCTION_ARGS) {
  bool local_model = PG_GETARG_BOOL(0);
  text* model_type_text = PG_GETARG_TEXT_PP(1);
  text* instance_name_text = PG_GETARG_TEXT_PP(2);
  text* api_key_text = PG_GETARG_TEXT_PP(3);
  text* config_text = PG_GETARG_TEXT_PP(4);

  std::string model_type(VARDATA_ANY(model_type_text), VARSIZE_ANY_EXHDR(model_type_text));
  std::string instance_name(VARDATA_ANY(instance_name_text), VARSIZE_ANY_EXHDR(instance_name_text));
  std::string api_key(VARDATA_ANY(api_key_text), VARSIZE_ANY_EXHDR(api_key_text));
  std::string config(VARDATA_ANY(config_text), VARSIZE_ANY_EXHDR(config_text));

  // insert into catalog table
  pg_llm_model_insert(local_model,
                      const_cast<char*>(model_type.c_str()),
                      const_cast<char*>(instance_name.c_str()),
                      const_cast<char*>(api_key.c_str()),
                      const_cast<char*>(config.c_str()));
  // register model
  auto& manager = pg_llm::ModelManager::get_instance();
  manager.register_model(model_type, [model_type]() {
    return std::make_unique<pg_llm::LLMInterface>(model_type);
  });

  bool success = manager.create_model_instance(local_model,
    model_type, instance_name, api_key, config);

  PG_RETURN_BOOL(success);
}

// Remove a model instance
Datum pg_llm_remove_model(PG_FUNCTION_ARGS) {
  text* instance_name_text = PG_GETARG_TEXT_PP(0);
  std::string instance_name(VARDATA_ANY(instance_name_text), VARSIZE_ANY_EXHDR(instance_name_text));

  auto& manager = pg_llm::ModelManager::get_instance();
  // delete from manager
  bool success = manager.remove_model_instance(instance_name);

  // delete from catalog
  pg_llm_model_delete(const_cast<char*>(instance_name.c_str()));
  PG_RETURN_BOOL(success);
}

// Single-turn chat with a specific model
Datum pg_llm_chat(PG_FUNCTION_ARGS) {
  text* instance_name_text = PG_GETARG_TEXT_PP(0);
  text* prompt_text = PG_GETARG_TEXT_PP(1);

  std::string instance_name(VARDATA_ANY(instance_name_text), VARSIZE_ANY_EXHDR(instance_name_text));
  std::string prompt(VARDATA_ANY(prompt_text), VARSIZE_ANY_EXHDR(prompt_text));

  auto& manager = pg_llm::ModelManager::get_instance();
  auto model = manager.get_model(instance_name);

  if (!model) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Model instance not found: %s", instance_name.c_str())));
  }

  auto response = model->chat_completion(prompt);
  text* result = cstring_to_text(response.response.c_str());
  PG_RETURN_TEXT_P(result);
}

// Parallel chat with multiple models
Datum pg_llm_parallel_chat(PG_FUNCTION_ARGS) {
  text* prompt_text = PG_GETARG_TEXT_PP(0);
  std::string prompt(VARDATA_ANY(prompt_text), VARSIZE_ANY_EXHDR(prompt_text));

  std::vector<std::string> model_names;

  if (PG_ARGISNULL(1)) {
    model_names = pg_llm_get_all_instancenames();
  } else {
    ArrayType* model_names_array = PG_GETARG_ARRAYTYPE_P(1);
    // Convert PostgreSQL array to vector of strings
    int num_models = ARR_DIMS(model_names_array)[0];
    Datum* elements;
    bool* nulls;
    int16 typlen;
    bool typbyval;
    char typalign;

    get_typlenbyvalalign(ARR_ELEMTYPE(model_names_array), &typlen, &typbyval, &typalign);
    deconstruct_array(model_names_array,
                      ARR_ELEMTYPE(model_names_array),
                      typlen,
                      typbyval,
                      typalign,
                      &elements,
                      &nulls,
                      &num_models);

    for (int i = 0; i < num_models; i++) {
      if (!nulls[i]) {
        text* model_name_text = (text*) elements[i];
        model_names.push_back(
          std::string(VARDATA_ANY(model_name_text), VARSIZE_ANY_EXHDR(model_name_text)));
      }
    }
  }

  auto& manager = pg_llm::ModelManager::get_instance();
  auto responses = manager.parallel_inference(prompt, model_names);
  auto best_response = manager.get_best_response(responses);

  text* result = cstring_to_text(best_response.response.c_str());
  PG_RETURN_TEXT_P(result);
}

Datum pg_llm_store_vector(PG_FUNCTION_ARGS) {
  text* table_name_text = PG_GETARG_TEXT_PP(0);
  text* column_name_text = PG_GETARG_TEXT_PP(1);
  int64 row_id = PG_GETARG_INT64(2);
  Datum vector_datum = PG_GETARG_DATUM(3);
  Jsonb* metadata = PG_ARGISNULL(4) ? NULL : PG_GETARG_JSONB_P(4);

  std::string table_name(VARDATA_ANY(table_name_text), VARSIZE_ANY_EXHDR(table_name_text));
  std::string column_name(VARDATA_ANY(column_name_text), VARSIZE_ANY_EXHDR(column_name_text));

  SPI_connect();
  char sql[1024];
  if (metadata) {
    snprintf(sql,
             sizeof(sql),
             "INSERT INTO _pg_llm_catalog.pg_llm_vectors (table_name, column_name, row_id, vector, metadata) "
             "VALUES ('%s', '%s', %ld, $1, $2) RETURNING id",
             table_name.c_str(),
             column_name.c_str(),
             row_id);
  } else {
    snprintf(sql,
             sizeof(sql),
             "INSERT INTO _pg_llm_catalog.pg_llm_vectors (table_name, column_name, row_id, vector) "
             "VALUES ('%s', '%s', %ld, $1) RETURNING id",
             table_name.c_str(),
             column_name.c_str(),
             row_id);
  }

  Oid argtypes[2] = {get_vector_type_oid(), JSONBOID};
  Datum values[2] = {vector_datum, PointerGetDatum(metadata)};
  char nulls[2] = {' ', metadata ? ' ' : 'n'};
  int nargs = metadata ? 2 : 1;

  SPIPlanPtr plan = SPI_prepare(sql, nargs, argtypes);
  if (plan == NULL) {
    SPI_finish();
    ereport(ERROR,
            (errcode(ERRCODE_INTERNAL_ERROR),
             errmsg("Failed to prepare SQL statement")));
  }

  int ret = SPI_execute_plan(plan, values, nulls, true, 0);
  if (ret != SPI_OK_SELECT) {
    elog(ERROR, "SPI_execute_plan failed: %d", ret);
  }

  SPITupleTable* tuptable = SPI_tuptable;
  if (tuptable == NULL || SPI_processed == 0) {
    SPI_finish();
    ereport(ERROR,
            (errcode(ERRCODE_INTERNAL_ERROR),
             errmsg("Failed to execute SQL statement")));
  }

  bool isnull;
  int64 id = DatumGetInt64(SPI_getbinval(tuptable->vals[0], tuptable->tupdesc, 1, &isnull));
  SPI_finish();

  PG_RETURN_INT64(id);
}

Datum pg_llm_search_vectors(PG_FUNCTION_ARGS) {
  Datum query_vector_datum = PG_GETARG_DATUM(0);
  int32 limit_count = PG_GETARG_INT32(1);
  float4 similarity_threshold = PG_GETARG_FLOAT4(2);

  FuncCallContext* funcctx;
  if (SRF_IS_FIRSTCALL()) {
    funcctx = SRF_FIRSTCALL_INIT();
    MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    SPI_connect();
    char sql[1024];
    snprintf(sql,
             sizeof(sql),
             "SELECT id, table_name, column_name, row_id, "
             "1 - (vector <=> $1) as similarity, metadata "
             "FROM _pg_llm_catalog.pg_llm_vectors "
             "WHERE 1 - (vector <=> $1) >= $2 "
             "ORDER BY vector <=> $1 "
             "LIMIT $3");

    Oid argtypes[3] = {get_vector_type_oid(), FLOAT4OID, INT4OID};
    Datum values[3] = {query_vector_datum,
                       Float4GetDatum(similarity_threshold),
                       Int32GetDatum(limit_count)
                      };
    char nulls[3] = {' ', ' ', ' '};

    SPIPlanPtr plan = SPI_prepare(sql, 3, argtypes);
    if (plan == NULL) {
      SPI_finish();
      ereport(ERROR,
              (errcode(ERRCODE_INTERNAL_ERROR),
               errmsg("Failed to prepare SQL statement")));
    }

    int ret = SPI_execute_plan(plan, values, nulls, true, 1);
    if (ret != SPI_OK_SELECT) {
      elog(ERROR, "SPI_execute_plan failed: %s", SPI_result_code_string(ret));
    }
    SPITupleTable* tuptable = SPI_tuptable;
    if (tuptable == NULL) {
      SPI_finish();
      ereport(ERROR,
              (errcode(ERRCODE_INTERNAL_ERROR),
               errmsg("Failed to execute SQL statement")));
    }

    funcctx->user_fctx = tuptable;
    funcctx->max_calls = SPI_processed;
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  SPITupleTable* tuptable = (SPITupleTable*)funcctx->user_fctx;

  if (funcctx->call_cntr < funcctx->max_calls) {
    HeapTuple tuple = tuptable->vals[funcctx->call_cntr];
    TupleDesc tupdesc = tuptable->tupdesc;

    Datum values[6];
    bool nulls[6] = {false};

    values[0] = SPI_getbinval(tuple, tupdesc, 1, &nulls[0]);
    values[1] = SPI_getbinval(tuple, tupdesc, 2, &nulls[1]);
    values[2] = SPI_getbinval(tuple, tupdesc, 3, &nulls[2]);
    values[3] = SPI_getbinval(tuple, tupdesc, 4, &nulls[3]);
    values[4] = SPI_getbinval(tuple, tupdesc, 5, &nulls[4]);
    values[5] = SPI_getbinval(tuple, tupdesc, 6, &nulls[5]);

    HeapTuple result = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(result));
  } else {
    SPI_finish();
    SRF_RETURN_DONE(funcctx);
  }
}

Datum pg_llm_get_embedding(PG_FUNCTION_ARGS) {
  text* instance_name_text = PG_GETARG_TEXT_PP(0);
  text* text_text = PG_GETARG_TEXT_PP(1);

  std::string instance_name(VARDATA_ANY(instance_name_text), VARSIZE_ANY_EXHDR(instance_name_text));
  std::string text(VARDATA_ANY(text_text), VARSIZE_ANY_EXHDR(text_text));

  auto& manager = pg_llm::ModelManager::get_instance();
  auto model = manager.get_model(instance_name);

  if (!model) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Model instance not found: %s", instance_name.c_str())));
  }

  auto embedding = model->get_embedding(text);
  return std_vector_to_vector(embedding);
}

static inline std::vector<pg_llm::text2sql::TableInfo> parse_schema_info(const std::string& schema_info) {
  Json::Value root;
  Json::Reader reader;
  std::vector<pg_llm::text2sql::TableInfo> result;

  if (!reader.parse(schema_info, root)) {
    PG_LLM_LOG_ERROR("Failed to parse schema info: %s", reader.getFormattedErrorMessages().c_str());
    return result;
  }

  // Check if tables field exists
  if (!root.isMember("tables") || !root["tables"].isArray()) {
    PG_LLM_LOG_ERROR("Invalid schema info format: missing or invalid 'tables' field");
    return result;
  }

  const Json::Value& tables = root["tables"];
  for (const auto& table : tables) {
    if (!table.isObject()) {
      continue;
    }

    // Get table name
    if (!table.isMember("name") || !table["name"].isString()) {
      continue;
    }

    pg_llm::text2sql::TableInfo table_info;
    table_info.name = table["name"].asString();

    // Get table description
    if (table.isMember("description") && table["description"].isString()) {
      table_info.description = table["description"].asString();
    }

    // Get column information
    if (!table.isMember("columns") || !table["columns"].isArray()) {
      continue;
    }

    const Json::Value& columns = table["columns"];
    for (const auto& column : columns) {
      if (!column.isObject()) {
        continue;
      }

      if (!column.isMember("name") || !column["name"].isString() ||
          !column.isMember("type") || !column["type"].isString()) {
        continue;
      }

      std::string col_name = column["name"].asString();
      std::string col_type = column["type"].asString();
      table_info.columns.push_back(std::make_pair(col_name, col_type));
    }

    result.push_back(table_info);
  }

  return result;
}

// Modify text2sql function to support vector search
Datum pg_llm_text2sql(PG_FUNCTION_ARGS) {
  text* instance_name = PG_GETARG_TEXT_PP(0);
  text* prompt = PG_GETARG_TEXT_PP(1);
  text* schema_info = PG_ARGISNULL(2) ? NULL : PG_GETARG_TEXT_PP(2);
  bool use_vector_search = PG_ARGISNULL(3) ? true : PG_GETARG_BOOL(3);

  try {
    // Get the model instance
    auto& manager = pg_llm::ModelManager::get_instance();
    auto model = manager.get_model(text_to_cstring(instance_name));
    if (!model) {
      ereport(ERROR,
              (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
               errmsg("Model instance not found: %s", text_to_cstring(instance_name))));
    }

    // create text2sql instance
    pg_llm::text2sql::Text2SQLConfig config;
    config.use_vector_search = use_vector_search;
    pg_llm::text2sql::Text2SQL text2sql(model, config);

    // get schema infos
    std::vector<pg_llm::text2sql::TableInfo> schema;
    if (schema_info) {
      // parse user define schema infos
      schema = parse_schema_info(text_to_cstring(schema_info));
    } else {
      // get schema infos from database
      schema = text2sql.get_schema_info();
    }

    // vector search
    std::vector<pg_llm::text2sql::VectorSearchResult> search_results;
    if (use_vector_search) {
      search_results = text2sql.search_vectors(text_to_cstring(prompt));
    }

    // generate sql
    std::string sql = text2sql.generate_sql(text_to_cstring(prompt), schema, search_results);

    // return result
    PG_RETURN_TEXT_P(cstring_to_text(sql.c_str()));
  } catch (const std::exception& e) {
    ereport(ERROR,
            (errcode(ERRCODE_INTERNAL_ERROR),
             errmsg("Failed to generate SQL: %s", e.what())));
  }
}

// Create a new chat session
Datum pg_llm_create_session(PG_FUNCTION_ARGS) {
  int max_messages = PG_ARGISNULL(0) ? 10 : PG_GETARG_INT32(0);
  auto& session_manager = pg_llm::SessionManager::get_instance();
  std::string session_id = session_manager.create_session(max_messages);
  PG_RETURN_TEXT_P(cstring_to_text(session_id.c_str()));
}

// multi chat
Datum pg_llm_multi_turn_chat(PG_FUNCTION_ARGS) {
  text* instance_name_text = PG_GETARG_TEXT_PP(0);
  text* session_id_text = PG_GETARG_TEXT_PP(1);
  text* prompt_text = PG_GETARG_TEXT_PP(2);

  std::string instance_name(VARDATA_ANY(instance_name_text), VARSIZE_ANY_EXHDR(instance_name_text));
  std::string session_id(VARDATA_ANY(session_id_text), VARSIZE_ANY_EXHDR(session_id_text));
  std::string prompt(VARDATA_ANY(prompt_text), VARSIZE_ANY_EXHDR(prompt_text));

  auto& session_manager = pg_llm::SessionManager::get_instance();
  auto session = session_manager.get_session(session_id);
  
  if (!session) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Session not found: %s", session_id.c_str())));
  }

  auto& manager = pg_llm::ModelManager::get_instance();
  auto model = manager.get_model(instance_name);

  if (!model) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Model instance not found: %s", instance_name.c_str())));
  }

  // Add user message to the conversation
  session_manager.add_message(session_id, {"user", prompt});

  // Get all messages in a conversation
  auto response = model->chat_completion(session->messages);

  // Add assistant replies to conversations
  session_manager.add_message(session_id, {"assistant", response.response});

  text* result = cstring_to_text(response.response.c_str());
  PG_RETURN_TEXT_P(result);
}

// Set maximum number of messages for a session
Datum pg_llm_set_max_messages(PG_FUNCTION_ARGS) {
  text *session_id = PG_GETARG_TEXT_P(0);
  int max_messages = PG_GETARG_INT32(1);
  
  auto& session_manager = pg_llm::SessionManager::get_instance();
  if (!session_manager.set_max_messages(text_to_cstring(session_id), max_messages)) {
      ereport(ERROR,
              (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
               errmsg("Session not found")));
  }
  
  PG_RETURN_VOID();
}

// Get information about all sessions
Datum pg_llm_get_sessions(PG_FUNCTION_ARGS) {
  FuncCallContext *funcctx;
  MemoryContext oldcontext;
  
  if (SRF_IS_FIRSTCALL()) {
      funcctx = SRF_FIRSTCALL_INIT();
      oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
      
      // Create tuple descriptor for return values
      TupleDesc tupdesc = CreateTemplateTupleDesc(4);
      TupleDescInitEntry(tupdesc, (AttrNumber) 1, "session_id", TEXTOID, -1, 0);
      TupleDescInitEntry(tupdesc, (AttrNumber) 2, "message_count", INT4OID, -1, 0);
      TupleDescInitEntry(tupdesc, (AttrNumber) 3, "max_messages", INT4OID, -1, 0);
      TupleDescInitEntry(tupdesc, (AttrNumber) 4, "last_active", TIMESTAMPTZOID, -1, 0);
      
      funcctx->tuple_desc = BlessTupleDesc(tupdesc);
      
      // Get all sessions
      auto& session_manager = pg_llm::SessionManager::get_instance();
      auto all_sessions = session_manager.get_all_sessions();
      funcctx->user_fctx = new std::vector<pg_llm::ChatSession>(all_sessions);
      funcctx->max_calls = all_sessions.size();
      
      MemoryContextSwitchTo(oldcontext);
  }
  
  funcctx = SRF_PERCALL_SETUP();
  auto sessions = static_cast<std::vector<pg_llm::ChatSession>*>(funcctx->user_fctx);
  
  if (funcctx->call_cntr < funcctx->max_calls) {
      const auto& session = (*sessions)[funcctx->call_cntr];
      
      Datum values[4];
      bool nulls[4] = {false, false, false, false};
      
      values[0] = CStringGetTextDatum(session.session_id.c_str());
      values[1] = Int32GetDatum(session.messages.size());
      values[2] = Int32GetDatum(session.max_messages);
      values[3] = TimestampTzGetDatum(session.last_active_time);
      
      HeapTuple tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
      SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
  } else {
      delete sessions;
      SRF_RETURN_DONE(funcctx);
  }
}

// Clean up expired sessions
Datum pg_llm_cleanup_sessions(PG_FUNCTION_ARGS) {
  int timeout_seconds = PG_GETARG_INT32(0);
  auto& session_manager = pg_llm::SessionManager::get_instance();
  session_manager.cleanup_expired_sessions(timeout_seconds);
  PG_RETURN_VOID();
}
