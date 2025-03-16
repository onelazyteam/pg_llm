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
PG_FUNCTION_INFO_V1(pg_llm_create_session);
PG_FUNCTION_INFO_V1(pg_llm_chat_session);

Datum pg_llm_add_model(PG_FUNCTION_ARGS);
Datum pg_llm_remove_model(PG_FUNCTION_ARGS);
Datum pg_llm_chat(PG_FUNCTION_ARGS);
Datum pg_llm_parallel_chat(PG_FUNCTION_ARGS);
Datum pg_llm_create_session(PG_FUNCTION_ARGS);
Datum pg_llm_chat_session(PG_FUNCTION_ARGS);

/* Extension initialization and finalization functions */
void _PG_init(void);
void _PG_fini(void);
}  // extern "C"

// Include our wrapper header
#include "catalog/pg_llm_models.h"

#include "models/chat_session.h"
#include "models/chatgpt_model.h"
#include "models/deepseek_model.h"
#include "models/hunyuan_model.h"
#include "models/llm_interface.h"
#include "models/model_manager.h"
#include "models/qianwen_model.h"

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

// Helper function to generate UUID
static std::string generate_uuid() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  std::uniform_int_distribution<> dis2(8, 11);

  std::stringstream ss;
  ss << std::hex;

  for (int i = 0; i < 8; i++) {
    ss << dis(gen);
  }
  ss << "-";
  for (int i = 0; i < 4; i++) {
    ss << dis(gen);
  }
  ss << "-4";  // Version 4
  for (int i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";
  ss << dis2(gen);  // Variant
  for (int i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";
  for (int i = 0; i < 12; i++) {
    ss << dis(gen);
  }

  return ss.str();
}

// Helper function to update session last_used_at
static void update_session_timestamp(const std::string& session_id) {
  SPI_connect();
  char sql[256];
  snprintf(sql,
           sizeof(sql),
           "UPDATE pg_llm_sessions SET last_used_at = CURRENT_TIMESTAMP WHERE session_id = '%s'",
           session_id.c_str());
  SPI_exec(sql, 0);
  SPI_finish();
}

// Helper function to add message to session history
static void add_session_message(const std::string& session_id,
                                const std::string& role,
                                const std::string& content) {
  SPI_connect();
  char sql[1024];
  snprintf(
      sql,
      sizeof(sql),
      "INSERT INTO pg_llm_session_history (session_id, role, content) VALUES ('%s', '%s', '%s')",
      session_id.c_str(),
      role.c_str(),
      content.c_str());
  SPI_exec(sql, 0);
  SPI_finish();
}

// Add a new model instance
Datum pg_llm_add_model(PG_FUNCTION_ARGS) {
  text* model_type_text = PG_GETARG_TEXT_PP(0);
  text* instance_name_text = PG_GETARG_TEXT_PP(1);
  text* api_key_text = PG_GETARG_TEXT_PP(2);
  text* config_text = PG_GETARG_TEXT_PP(3);

  std::string model_type(VARDATA_ANY(model_type_text), VARSIZE_ANY_EXHDR(model_type_text));
  std::string instance_name(VARDATA_ANY(instance_name_text), VARSIZE_ANY_EXHDR(instance_name_text));
  std::string api_key(VARDATA_ANY(api_key_text), VARSIZE_ANY_EXHDR(api_key_text));
  std::string config(VARDATA_ANY(config_text), VARSIZE_ANY_EXHDR(config_text));

  // insert into catalog table
  pg_llm_model_insert(const_cast<char*>(model_type.c_str()),
                      const_cast<char*>(instance_name.c_str()),
                      const_cast<char*>(api_key.c_str()),
                      const_cast<char*>(config.c_str()));
  // register model
  auto& manager = pg_llm::ModelManager::get_instance();
  manager.add_model_internal(model_type);
  bool success = manager.create_model_instance(model_type, instance_name, api_key, config);
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

// Create a new chat session
Datum pg_llm_create_session(PG_FUNCTION_ARGS) {
  text* model_instance_text = PG_GETARG_TEXT_PP(0);
  std::string model_instance(VARDATA_ANY(model_instance_text),
                             VARSIZE_ANY_EXHDR(model_instance_text));

  // Generate session ID if not provided
  std::string session_id;
  if (PG_NARGS() > 1 && !PG_ARGISNULL(1)) {
    text* session_id_text = PG_GETARG_TEXT_PP(1);
    session_id = std::string(VARDATA_ANY(session_id_text), VARSIZE_ANY_EXHDR(session_id_text));
  } else {
    session_id = generate_uuid();
  }

  // Verify model instance exists
  auto& manager = pg_llm::ModelManager::get_instance();
  auto model = manager.get_model(model_instance);
  if (!model) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Model instance not found: %s", model_instance.c_str())));
  }

  // Create session record
  SPI_connect();
  char sql[256];
  snprintf(sql,
           sizeof(sql),
           "INSERT INTO pg_llm_sessions (session_id, model_instance) VALUES ('%s', '%s')",
           session_id.c_str(),
           model_instance.c_str());
  SPI_exec(sql, 0);
  SPI_finish();

  text* result = cstring_to_text(session_id.c_str());
  PG_RETURN_TEXT_P(result);
}

// Chat within a session
Datum pg_llm_chat_session(PG_FUNCTION_ARGS) {
  text* session_id_text = PG_GETARG_TEXT_PP(0);
  text* prompt_text = PG_GETARG_TEXT_PP(1);

  std::string session_id(VARDATA_ANY(session_id_text), VARSIZE_ANY_EXHDR(session_id_text));
  std::string prompt(VARDATA_ANY(prompt_text), VARSIZE_ANY_EXHDR(prompt_text));

  // Get session information
  SPI_connect();
  char sql[256];
  snprintf(sql,
           sizeof(sql),
           "SELECT model_instance FROM pg_llm_sessions WHERE session_id = '%s'",
           session_id.c_str());

  SPITupleTable* tuptable;
  uint64 proc = SPI_exec(sql, 0);
  if (proc != SPI_OK_SELECT || SPI_processed == 0) {
    SPI_finish();
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Session not found: %s", session_id.c_str())));
  }

  tuptable = SPI_tuptable;
  char* model_instance = SPI_getvalue(tuptable->vals[0], tuptable->tupdesc, 1);
  SPI_finish();

  // Get model
  auto& manager = pg_llm::ModelManager::get_instance();
  auto model = manager.get_model(model_instance);
  if (!model) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Model instance not found: %s", model_instance)));
  }

  // Get session history
  std::vector<pg_llm::ChatMessage> messages;
  SPI_connect();
  snprintf(
      sql,
      sizeof(sql),
      "SELECT role, content FROM pg_llm_session_history WHERE session_id = '%s' ORDER BY id ASC",
      session_id.c_str());

  proc = SPI_exec(sql, 0);
  if (proc == SPI_OK_SELECT) {
    tuptable = SPI_tuptable;
    for (uint64 i = 0; i < SPI_processed; i++) {
      char* role = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 1);
      char* content = SPI_getvalue(tuptable->vals[i], tuptable->tupdesc, 2);
      messages.push_back({role, content});
    }
  }
  SPI_finish();

  // Add user message to history
  add_session_message(session_id, "user", prompt);
  messages.push_back({"user", prompt});

  // Get response from model
  auto response = model->chat_completion(messages);

  // Add assistant response to history
  add_session_message(session_id, "assistant", response.response);

  // Update session timestamp
  update_session_timestamp(session_id);

  text* result = cstring_to_text(response.response.c_str());
  PG_RETURN_TEXT_P(result);
}

// Parallel chat with multiple models
Datum pg_llm_parallel_chat(PG_FUNCTION_ARGS) {
  text* prompt_text = PG_GETARG_TEXT_PP(0);
  ArrayType* model_names_array = PG_GETARG_ARRAYTYPE_P(1);

  std::string prompt(VARDATA_ANY(prompt_text), VARSIZE_ANY_EXHDR(prompt_text));

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

  std::vector<std::string> model_names;
  for (int i = 0; i < num_models; i++) {
    if (!nulls[i]) {
      text* model_name_text = (text*) elements[i];
      model_names.push_back(
          std::string(VARDATA_ANY(model_name_text), VARSIZE_ANY_EXHDR(model_name_text)));
    }
  }

  auto& manager = pg_llm::ModelManager::get_instance();
  auto responses = manager.parallel_inference(prompt, model_names);
  auto best_response = manager.get_best_response(responses);

  text* result = cstring_to_text(best_response.response.c_str());
  PG_RETURN_TEXT_P(result);
}
