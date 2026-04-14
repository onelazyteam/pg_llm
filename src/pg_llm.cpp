extern "C" {
#include "postgres.h"  // clang-format off
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"
#include "utils/lsyscache.h"
#include "utils/timestamp.h"
#include "utils/uuid.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

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
PG_FUNCTION_INFO_V1(pg_llm_chat_stream);
PG_FUNCTION_INFO_V1(pg_llm_multi_turn_chat_stream);
PG_FUNCTION_INFO_V1(pg_llm_chat_json);
PG_FUNCTION_INFO_V1(pg_llm_parallel_chat_json);
PG_FUNCTION_INFO_V1(pg_llm_text2sql_json);
PG_FUNCTION_INFO_V1(pg_llm_execute_sql_with_analysis);
PG_FUNCTION_INFO_V1(pg_llm_generate_report);
PG_FUNCTION_INFO_V1(pg_llm_get_session);
PG_FUNCTION_INFO_V1(pg_llm_get_session_messages);
PG_FUNCTION_INFO_V1(pg_llm_update_session_state);
PG_FUNCTION_INFO_V1(pg_llm_delete_session);
PG_FUNCTION_INFO_V1(pg_llm_add_knowledge);
PG_FUNCTION_INFO_V1(pg_llm_search_knowledge);
PG_FUNCTION_INFO_V1(pg_llm_record_feedback);
PG_FUNCTION_INFO_V1(pg_llm_get_audit_log);
PG_FUNCTION_INFO_V1(pg_llm_get_trace);

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
Datum pg_llm_chat_stream(PG_FUNCTION_ARGS);
Datum pg_llm_multi_turn_chat_stream(PG_FUNCTION_ARGS);
Datum pg_llm_chat_json(PG_FUNCTION_ARGS);
Datum pg_llm_parallel_chat_json(PG_FUNCTION_ARGS);
Datum pg_llm_text2sql_json(PG_FUNCTION_ARGS);
Datum pg_llm_execute_sql_with_analysis(PG_FUNCTION_ARGS);
Datum pg_llm_generate_report(PG_FUNCTION_ARGS);
Datum pg_llm_get_session(PG_FUNCTION_ARGS);
Datum pg_llm_get_session_messages(PG_FUNCTION_ARGS);
Datum pg_llm_update_session_state(PG_FUNCTION_ARGS);
Datum pg_llm_delete_session(PG_FUNCTION_ARGS);
Datum pg_llm_add_knowledge(PG_FUNCTION_ARGS);
Datum pg_llm_search_knowledge(PG_FUNCTION_ARGS);
Datum pg_llm_record_feedback(PG_FUNCTION_ARGS);
Datum pg_llm_get_audit_log(PG_FUNCTION_ARGS);
Datum pg_llm_get_trace(PG_FUNCTION_ARGS);

void _PG_init(void);
void _PG_fini(void);
}  // extern "C"

#include "catalog/pg_llm_models.h"
#include "models/llm_interface.h"
#include "models/model_manager.h"
#include "text2sql/pg_vector.h"
#include "text2sql/text2sql.h"
#include "utils/pg_llm_glog.h"
#include "utils/pg_llm_support.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

using pg_llm::ChatMessage;
using pg_llm::LLMInterface;
using pg_llm::ModelManager;
using pg_llm::ModelResponse;
using pg_llm::StreamResponse;

struct ChatExecutionResult {
  std::string request_id;
  std::string selected_instance;
  std::string selected_model_name;
  std::string response;
  double confidence_score = 0.0;
  bool fallback_used = false;
  std::string fallback_instance;
  Json::Value candidates = Json::arrayValue;
  Json::Value trace_events = Json::arrayValue;
};

struct StreamSrfState {
  std::vector<pg_llm::StreamChunk> chunks;
  std::string request_id;
  std::string model_name;
  double confidence_score = 0.0;
};

struct SessionMessageRow {
  int64 id = 0;
  std::string request_id;
  std::string role;
  std::string content;
  Datum created_at = 0;
};

struct KnowledgeSearchRow {
  int64 chunk_id = 0;
  int64 document_id = 0;
  int32 chunk_index = 0;
  std::string source_name;
  std::string content;
  float similarity = 0.0f;
  std::string metadata_json = "{}";
};

struct AuditRow {
  std::string request_id;
  std::string event_type;
  std::string instance_name;
  std::string session_id;
  bool success = false;
  double confidence_score = 0.0;
  std::string metadata_json = "{}";
  Datum created_at = 0;
};

std::string text_to_std_string(text* value) {
  return std::string(VARDATA_ANY(value), VARSIZE_ANY_EXHDR(value));
}

Json::Value jsonb_to_value(Jsonb* value) {
  if (value == nullptr) {
    return Json::Value(Json::objectValue);
  }
  Datum text_datum = DirectFunctionCall1(jsonb_out, PointerGetDatum(value));
  return pg_llm_parse_json(DatumGetCString(text_datum));
}

Datum json_to_jsonb_datum(const Json::Value& value) {
  return pg_llm_jsonb_from_string(pg_llm_write_json(value));
}

std::string datum_to_string(Datum value, Oid type_oid, bool is_null) {
  if (is_null) {
    return "";
  }

  Oid output_func = InvalidOid;
  bool varlena = false;
  getTypeOutputInfo(type_oid, &output_func, &varlena);
  char* output = OidOutputFunctionCall(output_func, value);
  return output ? output : "";
}

void ensure_spi_result(int code, int expected, const char* message) {
  if (code != expected) {
    ereport(ERROR,
            (errcode(ERRCODE_INTERNAL_ERROR),
             errmsg("%s: %s", message, SPI_result_code_string(code))));
  }
}

Datum text_datum(const std::string& value) {
  return CStringGetTextDatum(value.c_str());
}

Datum uuid_text_datum(const std::string& value) {
  return CStringGetTextDatum(value.c_str());
}

std::vector<float> deterministic_embedding(const std::string& text, int dimensions = 64) {
  std::vector<float> embedding(dimensions, 0.0f);
  std::hash<std::string> hasher;
  for (int i = 0; i < dimensions; ++i) {
    size_t hashed = hasher(text + ":" + std::to_string(i));
    embedding[i] = static_cast<float>((hashed % 2000) - 1000) / 1000.0f;
  }
  return embedding;
}

Json::Value redact_metadata(const Json::Value& input) {
  if (!pg_llm_redact_sensitive) {
    return input;
  }

  Json::Value output = input;
  if (output.isMember("prompt")) {
    output["prompt"] = pg_llm_redact_secret(output["prompt"].asString());
  }
  if (output.isMember("response")) {
    output["response"] = pg_llm_redact_secret(output["response"].asString());
  }
  if (output.isMember("api_key")) {
    output["api_key"] = pg_llm_redact_secret(output["api_key"].asString());
  }
  return output;
}

void insert_trace_log(const std::string& request_id,
                      const std::string& stage,
                      const Json::Value& details) {
  if (!pg_llm_trace_enabled) {
    return;
  }

  SPI_connect();
  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_trace_log (request_id, stage, details) "
    "VALUES ($1::uuid, $2, $3::jsonb)";
  Oid argtypes[3] = {TEXTOID, TEXTOID, TEXTOID};
  Datum values[3] = {
    uuid_text_datum(request_id),
    text_datum(stage),
    text_datum(pg_llm_write_json(redact_metadata(details)))};
  char nulls[3] = {' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql, 3, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_INSERT, "failed to persist trace log");
  SPI_finish();
}

void insert_audit_log(const std::string& request_id,
                      const std::string& event_type,
                      const std::string& instance_name,
                      const std::string& session_id,
                      bool success,
                      double confidence_score,
                      const Json::Value& metadata) {
  if (!pg_llm_audit_enabled || pg_llm_audit_sample_rate <= 0.0) {
    return;
  }

  SPI_connect();
  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_audit_log "
    "(request_id, event_type, instance_name, session_id, success, confidence_score, metadata) "
    "VALUES ($1::uuid, $2, $3, $4, $5, $6, $7::jsonb)";
  Oid argtypes[7] = {TEXTOID, TEXTOID, TEXTOID, TEXTOID, BOOLOID, FLOAT8OID, TEXTOID};
  Datum values[7] = {
    uuid_text_datum(request_id),
    text_datum(event_type),
    text_datum(instance_name),
    text_datum(session_id),
    BoolGetDatum(success),
    Float8GetDatum(confidence_score),
    text_datum(pg_llm_write_json(redact_metadata(metadata)))};
  char nulls[7] = {' ', ' ', ' ', ' ', ' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql, 7, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_INSERT, "failed to persist audit log");
  SPI_finish();
}

PgLlmModelInfo get_model_info_or_error(const std::string& instance_name) {
  PgLlmModelInfo info;
  if (!pg_llm_model_get_info(instance_name, &info)) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Model instance not found: %s", instance_name.c_str())));
  }
  return info;
}

std::shared_ptr<LLMInterface> get_model_or_error(const std::string& instance_name) {
  auto& manager = ModelManager::get_instance();
  auto model = manager.get_model(instance_name);
  if (!model) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("Model instance not found or could not be loaded: %s",
                    instance_name.c_str())));
  }
  return model;
}

Json::Value get_session_json_internal(const std::string& session_id, bool strict) {
  SPI_connect();
  const char* sql =
    "SELECT session_id, state::text, max_messages, last_active_at, created_at "
    "FROM _pg_llm_catalog.pg_llm_sessions WHERE session_id = $1";
  Oid argtypes[1] = {TEXTOID};
  Datum values[1] = {text_datum(session_id)};
  char nulls[1] = {' '};
  int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, true, 1);
  ensure_spi_result(ret, SPI_OK_SELECT, "failed to fetch session");
  if (SPI_processed == 0) {
    SPI_finish();
    if (strict) {
      ereport(ERROR,
              (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
               errmsg("Session not found: %s", session_id.c_str())));
    }
    return Json::Value(Json::nullValue);
  }

  HeapTuple tuple = SPI_tuptable->vals[0];
  TupleDesc tupdesc = SPI_tuptable->tupdesc;
  bool isnull = false;
  Json::Value result(Json::objectValue);
  result["session_id"] = SPI_getvalue(tuple, tupdesc, 1);
  result["state"] = pg_llm_parse_json(SPI_getvalue(tuple, tupdesc, 2));
  result["max_messages"] = DatumGetInt32(SPI_getbinval(tuple, tupdesc, 3, &isnull));
  result["last_active_at"] = datum_to_string(SPI_getbinval(tuple, tupdesc, 4, &isnull),
                                              TIMESTAMPTZOID,
                                              isnull);
  result["created_at"] = datum_to_string(SPI_getbinval(tuple, tupdesc, 5, &isnull),
                                          TIMESTAMPTZOID,
                                          isnull);
  SPI_finish();
  return result;
}

void trim_session_messages(const std::string& session_id, int max_messages) {
  SPI_connect();
  const char* sql =
    "DELETE FROM _pg_llm_catalog.pg_llm_session_messages "
    "WHERE id IN ("
    "  SELECT id FROM _pg_llm_catalog.pg_llm_session_messages "
    "  WHERE session_id = $1 "
    "  ORDER BY id DESC OFFSET $2"
    ")";
  Oid argtypes[2] = {TEXTOID, INT4OID};
  Datum values[2] = {text_datum(session_id), Int32GetDatum(max_messages)};
  char nulls[2] = {' ', ' '};
  int ret = SPI_execute_with_args(sql, 2, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_DELETE, "failed to trim session messages");
  SPI_finish();
}

void append_session_message(const std::string& session_id,
                            const std::string& request_id,
                            const std::string& role,
                            const std::string& content) {
  Json::Value session = get_session_json_internal(session_id, true);

  SPI_connect();
  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_session_messages "
    "(session_id, request_id, role, content) VALUES ($1, $2::uuid, $3, $4)";
  Oid argtypes[4] = {TEXTOID, TEXTOID, TEXTOID, TEXTOID};
  Datum values[4] = {
    text_datum(session_id),
    uuid_text_datum(request_id),
    text_datum(role),
    text_datum(content)};
  char nulls[4] = {' ', ' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql, 4, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_INSERT, "failed to append session message");
  SPI_finish();

  trim_session_messages(session_id, session["max_messages"].asInt());

  SPI_connect();
  const char* update_sql =
    "UPDATE _pg_llm_catalog.pg_llm_sessions "
    "SET last_active_at = CURRENT_TIMESTAMP WHERE session_id = $1";
  Oid update_types[1] = {TEXTOID};
  Datum update_values[1] = {text_datum(session_id)};
  char update_nulls[1] = {' '};
  ret = SPI_execute_with_args(update_sql, 1, update_types, update_values, update_nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_UPDATE, "failed to update session activity");
  SPI_finish();
}

std::vector<ChatMessage> load_session_messages(const std::string& session_id) {
  get_session_json_internal(session_id, true);
  SPI_connect();
  const char* sql =
    "SELECT role, content FROM _pg_llm_catalog.pg_llm_session_messages "
    "WHERE session_id = $1 ORDER BY id";
  Oid argtypes[1] = {TEXTOID};
  Datum values[1] = {text_datum(session_id)};
  char nulls[1] = {' '};
  int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, true, 0);
  ensure_spi_result(ret, SPI_OK_SELECT, "failed to load session messages");

  std::vector<ChatMessage> messages;
  for (uint64 i = 0; i < SPI_processed; ++i) {
    messages.push_back(ChatMessage{
      SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1),
      SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2)});
  }
  SPI_finish();
  return messages;
}

Json::Value get_session_messages_json(const std::string& session_id) {
  SPI_connect();
  const char* sql =
    "SELECT id, request_id::text, role, content, created_at "
    "FROM _pg_llm_catalog.pg_llm_session_messages "
    "WHERE session_id = $1 ORDER BY id";
  Oid argtypes[1] = {TEXTOID};
  Datum values[1] = {text_datum(session_id)};
  char nulls[1] = {' '};
  int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, true, 0);
  ensure_spi_result(ret, SPI_OK_SELECT, "failed to list session messages");

  Json::Value rows(Json::arrayValue);
  for (uint64 i = 0; i < SPI_processed; ++i) {
    HeapTuple tuple = SPI_tuptable->vals[i];
    TupleDesc tupdesc = SPI_tuptable->tupdesc;
    bool isnull = false;
    Json::Value row(Json::objectValue);
    row["id"] = Json::Int64(DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isnull)));
    row["request_id"] = SPI_getvalue(tuple, tupdesc, 2);
    row["role"] = SPI_getvalue(tuple, tupdesc, 3);
    row["content"] = SPI_getvalue(tuple, tupdesc, 4);
    row["created_at"] = datum_to_string(SPI_getbinval(tuple, tupdesc, 5, &isnull),
                                         TIMESTAMPTZOID,
                                         isnull);
    rows.append(row);
  }

  SPI_finish();
  return rows;
}

std::string create_session_internal(int max_messages) {
  std::string session_id = pg_llm_generate_uuid();
  SPI_connect();
  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_sessions "
    "(session_id, max_messages, state) VALUES ($1, $2, '{}'::jsonb)";
  Oid argtypes[2] = {TEXTOID, INT4OID};
  Datum values[2] = {text_datum(session_id), Int32GetDatum(max_messages)};
  char nulls[2] = {' ', ' '};
  int ret = SPI_execute_with_args(sql, 2, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_INSERT, "failed to create session");
  SPI_finish();
  return session_id;
}

bool delete_session_internal(const std::string& session_id) {
  SPI_connect();
  const char* delete_messages =
    "DELETE FROM _pg_llm_catalog.pg_llm_session_messages WHERE session_id = $1";
  Oid argtypes[1] = {TEXTOID};
  Datum values[1] = {text_datum(session_id)};
  char nulls[1] = {' '};
  int ret = SPI_execute_with_args(delete_messages, 1, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_DELETE, "failed to delete session messages");
  SPI_finish();

  SPI_connect();
  const char* delete_session =
    "DELETE FROM _pg_llm_catalog.pg_llm_sessions WHERE session_id = $1";
  ret = SPI_execute_with_args(delete_session, 1, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_DELETE, "failed to delete session");
  uint64 affected = SPI_processed;
  SPI_finish();
  return affected > 0;
}

void update_session_state_internal(const std::string& session_id, const Json::Value& state) {
  get_session_json_internal(session_id, true);
  SPI_connect();
  const char* sql =
    "UPDATE _pg_llm_catalog.pg_llm_sessions "
    "SET state = $2::jsonb, last_active_at = CURRENT_TIMESTAMP "
    "WHERE session_id = $1";
  Oid argtypes[2] = {TEXTOID, TEXTOID};
  Datum values[2] = {text_datum(session_id), text_datum(pg_llm_write_json(state))};
  char nulls[2] = {' ', ' '};
  int ret = SPI_execute_with_args(sql, 2, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_UPDATE, "failed to update session state");
  SPI_finish();
}

void cleanup_sessions_internal(int timeout_seconds) {
  SPI_connect();
  const char* sql =
    "DELETE FROM _pg_llm_catalog.pg_llm_sessions "
    "WHERE last_active_at < CURRENT_TIMESTAMP - make_interval(secs => $1)";
  Oid argtypes[1] = {INT4OID};
  Datum values[1] = {Int32GetDatum(timeout_seconds)};
  char nulls[1] = {' '};
  int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_DELETE, "failed to cleanup sessions");
  SPI_finish();
}

std::string build_rag_context(const std::string& query, int limit);

ChatExecutionResult maybe_apply_fallback(const ChatExecutionResult& input,
                                         const Json::Value& options,
                                         const std::string& event_type) {
  PgLlmModelInfo selected_info = get_model_info_or_error(input.selected_instance);
  double threshold = selected_info.confidence_threshold > 0.0
    ? selected_info.confidence_threshold
    : pg_llm_default_confidence_threshold;

  if (options.isObject() && options.isMember("confidence_threshold")) {
    threshold = options["confidence_threshold"].asDouble();
  }

  std::string fallback_instance = selected_info.fallback_instance;
  if (fallback_instance.empty() && pg_llm_default_local_fallback != nullptr) {
    fallback_instance = pg_llm_default_local_fallback;
  }
  if (options.isObject() && options.isMember("fallback_instance")) {
    fallback_instance = options["fallback_instance"].asString();
  }

  if (input.confidence_score >= threshold || fallback_instance.empty() ||
      fallback_instance == input.selected_instance) {
    return input;
  }

  auto fallback_model = get_model_or_error(fallback_instance);
  auto fallback_response = fallback_model->chat_completion(input.response);
  ChatExecutionResult result = input;
  result.fallback_used = true;
  result.fallback_instance = fallback_instance;
  result.selected_instance = fallback_instance;
  result.selected_model_name = fallback_response.model_name;
  result.response = fallback_response.response;
  result.confidence_score = fallback_response.confidence_score;

  Json::Value trace(Json::objectValue);
  trace["event_type"] = event_type;
  trace["reason"] = "confidence_below_threshold";
  trace["threshold"] = threshold;
  trace["fallback_instance"] = fallback_instance;
  result.trace_events.append(trace);
  return result;
}

ChatExecutionResult execute_single_chat_internal(const std::string& instance_name,
                                                 const std::string& prompt,
                                                 const Json::Value& options,
                                                 const std::optional<std::string>& session_id,
                                                 bool streaming) {
  auto model = get_model_or_error(instance_name);
  std::string request_id = pg_llm_generate_uuid();

  std::vector<ChatMessage> messages;
  if (session_id.has_value()) {
    messages = load_session_messages(*session_id);
  }

  std::string effective_prompt = prompt;
  if (options.get("enable_rag", false).asBool()) {
    std::string rag_context = build_rag_context(prompt, options.get("knowledge_limit", 3).asInt());
    if (!rag_context.empty()) {
      effective_prompt += "\n\nKnowledge Context:\n" + rag_context;
    }
  }
  messages.push_back(ChatMessage{"user", effective_prompt});

  ModelResponse response;
  if (streaming) {
    auto stream_response = model->stream_chat_completion(messages);
    response = ModelResponse{stream_response.response,
                             stream_response.confidence_score,
                             stream_response.model_name};
  } else {
    response = model->chat_completion(messages);
  }

  ChatExecutionResult result;
  result.request_id = request_id;
  result.selected_instance = instance_name;
  result.selected_model_name = response.model_name;
  result.response = response.response;
  result.confidence_score = response.confidence_score;

  Json::Value candidate(Json::objectValue);
  candidate["instance_name"] = instance_name;
  candidate["model_name"] = response.model_name;
  candidate["response"] = response.response;
  candidate["confidence_score"] = response.confidence_score;
  result.candidates.append(candidate);

  Json::Value trace(Json::objectValue);
  trace["prompt"] = prompt;
  trace["instance_name"] = instance_name;
  trace["confidence_score"] = response.confidence_score;
  trace["streaming"] = streaming;
  if (options.get("enable_rag", false).asBool()) {
    trace["rag_enabled"] = true;
  }
  result.trace_events.append(trace);

  result = maybe_apply_fallback(result, options, session_id.has_value() ? "multi_turn_chat" : "chat");

  if (session_id.has_value()) {
    append_session_message(*session_id, request_id, "user", prompt);
    append_session_message(*session_id, request_id, "assistant", result.response);
  }

  Json::Value audit(Json::objectValue);
  audit["prompt"] = prompt;
  audit["response"] = result.response;
  audit["fallback_used"] = result.fallback_used;
  audit["selected_model_name"] = result.selected_model_name;
  insert_audit_log(request_id,
                   session_id.has_value() ? "multi_turn_chat" : "chat",
                   result.selected_instance,
                   session_id.value_or(""),
                   true,
                   result.confidence_score,
                   audit);
  for (const auto& item : result.trace_events) {
    insert_trace_log(request_id, "chat", item);
  }

  return result;
}

ChatExecutionResult execute_parallel_chat_internal(const std::string& prompt,
                                                   const std::vector<std::string>& model_names,
                                                   const Json::Value& options) {
  auto& manager = ModelManager::get_instance();
  auto responses = manager.parallel_inference(prompt, model_names);
  if (responses.empty()) {
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("No model responses available for parallel chat")));
  }

  ChatExecutionResult result;
  result.request_id = pg_llm_generate_uuid();
  ModelResponse best = responses.front();
  std::string best_instance = model_names.front();
  for (size_t i = 0; i < responses.size(); ++i) {
    const auto& response = responses[i];
    Json::Value candidate(Json::objectValue);
    candidate["instance_name"] = i < model_names.size() ? model_names[i] : response.model_name;
    candidate["model_name"] = response.model_name;
    candidate["response"] = response.response;
    candidate["confidence_score"] = response.confidence_score;
    result.candidates.append(candidate);

    if (response.confidence_score > best.confidence_score) {
      best = response;
      best_instance = i < model_names.size() ? model_names[i] : response.model_name;
    }
  }

  result.selected_instance = best_instance;
  result.selected_model_name = best.model_name;
  result.response = best.response;
  result.confidence_score = best.confidence_score;

  Json::Value trace(Json::objectValue);
  trace["prompt"] = prompt;
  trace["candidate_count"] = static_cast<int>(responses.size());
  trace["selected_instance"] = best_instance;
  trace["confidence_score"] = best.confidence_score;
  result.trace_events.append(trace);
  result = maybe_apply_fallback(result, options, "parallel_chat");

  Json::Value audit(Json::objectValue);
  audit["prompt"] = prompt;
  audit["response"] = result.response;
  audit["fallback_used"] = result.fallback_used;
  audit["candidates"] = result.candidates;
  insert_audit_log(result.request_id,
                   "parallel_chat",
                   result.selected_instance,
                   "",
                   true,
                   result.confidence_score,
                   audit);
  for (const auto& item : result.trace_events) {
    insert_trace_log(result.request_id, "parallel_chat", item);
  }

  return result;
}

pg_llm::text2sql::Text2SQLConfig parse_text2sql_config(bool use_vector_search,
                                                       const Json::Value& options) {
  pg_llm::text2sql::Text2SQLConfig config;
  config.use_vector_search = use_vector_search;
  if (options.isObject()) {
    if (options.isMember("enable_cache")) {
      config.enable_cache = options["enable_cache"].asBool();
    }
    if (options.isMember("cache_ttl_seconds")) {
      config.cache_ttl_seconds = options["cache_ttl_seconds"].asInt();
    }
    if (options.isMember("parallel_processing")) {
      config.parallel_processing = options["parallel_processing"].asBool();
    }
    if (options.isMember("max_parallel_threads")) {
      config.max_parallel_threads = options["max_parallel_threads"].asInt();
    }
    if (options.isMember("similarity_threshold")) {
      config.similarity_threshold = options["similarity_threshold"].asFloat();
    }
    if (options.isMember("sample_data_limit")) {
      config.sample_data_limit = options["sample_data_limit"].asInt();
    }
  }
  return config;
}

std::vector<pg_llm::text2sql::TableInfo> parse_schema_info(const std::string& schema_info) {
  Json::Value root = pg_llm_parse_json(schema_info);
  std::vector<pg_llm::text2sql::TableInfo> result;
  if (!root.isObject() || !root.isMember("tables") || !root["tables"].isArray()) {
    return result;
  }

  for (const auto& table : root["tables"]) {
    if (!table.isObject()) {
      continue;
    }
    pg_llm::text2sql::TableInfo info;
    info.name = table.get("name", "").asString();
    info.description = table.get("description", "").asString();
    for (const auto& column : table["columns"]) {
      info.columns.emplace_back(column.get("name", "").asString(),
                                column.get("type", "").asString());
    }
    result.push_back(info);
  }

  return result;
}

Json::Value build_sql_result_json(const std::string& sql) {
  Json::Value result(Json::objectValue);
  result["sql"] = sql;

  SPI_connect();
  int ret = SPI_execute(sql.c_str(), true, 0);
  if (ret != SPI_OK_SELECT && ret != SPI_OK_INSERT && ret != SPI_OK_UPDATE &&
      ret != SPI_OK_DELETE && ret != SPI_OK_UTILITY) {
    SPI_finish();
    ereport(ERROR,
            (errcode(ERRCODE_INVALID_SQL_STATEMENT_NAME),
             errmsg("Failed to execute SQL: %s", SPI_result_code_string(ret))));
  }

  result["status"] = SPI_result_code_string(ret);
  result["row_count"] = Json::UInt64(SPI_processed);

  if (ret == SPI_OK_SELECT && SPI_tuptable != nullptr) {
    Json::Value columns(Json::arrayValue);
    Json::Value rows(Json::arrayValue);
    TupleDesc tupdesc = SPI_tuptable->tupdesc;

    for (int column = 0; column < tupdesc->natts; ++column) {
      columns.append(NameStr(TupleDescAttr(tupdesc, column)->attname));
    }

    for (uint64 row_index = 0; row_index < SPI_processed; ++row_index) {
      Json::Value row(Json::objectValue);
      for (int column = 0; column < tupdesc->natts; ++column) {
        bool isnull = false;
        Datum datum = SPI_getbinval(SPI_tuptable->vals[row_index], tupdesc, column + 1, &isnull);
        row[NameStr(TupleDescAttr(tupdesc, column)->attname)] =
          datum_to_string(datum, TupleDescAttr(tupdesc, column)->atttypid, isnull);
      }
      rows.append(row);
    }

    result["columns"] = columns;
    result["rows"] = rows;
  }

  SPI_finish();

  SPI_connect();
  const std::string explain_sql = "EXPLAIN " + sql;
  ret = SPI_execute(explain_sql.c_str(), false, 0);
  if (ret == SPI_OK_SELECT && SPI_tuptable != nullptr) {
    Json::Value explain(Json::arrayValue);
    for (uint64 i = 0; i < SPI_processed; ++i) {
      explain.append(SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1));
    }
    result["explain"] = explain;
  }
  SPI_finish();

  return result;
}

Json::Value build_text2sql_json_internal(const std::string& instance_name,
                                         const std::string& prompt,
                                         const std::optional<std::string>& schema_info,
                                         bool use_vector_search,
                                         const Json::Value& options) {
  auto model = get_model_or_error(instance_name);
  pg_llm::text2sql::Text2SQLConfig config = parse_text2sql_config(use_vector_search, options);
  pg_llm::text2sql::Text2SQL text2sql(model, config);

  std::vector<pg_llm::text2sql::TableInfo> schema =
    schema_info.has_value() ? parse_schema_info(*schema_info) : text2sql.get_schema_info();
  std::vector<pg_llm::text2sql::VectorSchemaInfo> search_results;
  std::vector<std::string> similar_queries;
  if (use_vector_search) {
    search_results = text2sql.search_vectors(prompt);
    similar_queries = text2sql.get_similar_queries(prompt);
  }

  std::string sql = text2sql.generate_statement(prompt, schema, search_results, similar_queries);
  Json::Value execution = build_sql_result_json(sql);

  Json::Value result(Json::objectValue);
  result["request_id"] = pg_llm_generate_uuid();
  result["instance_name"] = instance_name;
  result["prompt"] = prompt;
  result["generated_sql"] = sql;
  result["execution"] = execution;
  result["similar_queries"] = Json::arrayValue;
  for (const auto& entry : similar_queries) {
    result["similar_queries"].append(entry);
  }
  result["vector_hits"] = Json::arrayValue;
  for (const auto& hit : search_results) {
    Json::Value value(Json::objectValue);
    value["table_name"] = hit.table_name;
    value["column_name"] = hit.column_name;
    value["row_id"] = Json::Int64(hit.row_id);
    value["similarity"] = hit.similarity;
    value["metadata"] = hit.metadata;
    result["vector_hits"].append(value);
  }

  Json::Value audit(Json::objectValue);
  audit["prompt"] = prompt;
  audit["response"] = sql;
  audit["vector_search"] = use_vector_search;
  insert_audit_log(result["request_id"].asString(),
                   "text2sql",
                   instance_name,
                   "",
                   true,
                   1.0,
                   audit);
  insert_trace_log(result["request_id"].asString(), "text2sql", result);
  return result;
}

Json::Value build_report_json_internal(const std::string& instance_name,
                                       const std::string& sql,
                                       const Json::Value& options) {
  Json::Value execution = build_sql_result_json(sql);
  auto model = get_model_or_error(instance_name);

  Json::Value report(Json::objectValue);
  report["request_id"] = pg_llm_generate_uuid();
  report["sql"] = sql;
  report["execution"] = execution;
  report["summary"] = Json::objectValue;
  report["summary"]["row_count"] = execution["row_count"];
  report["summary"]["column_count"] = execution.isMember("columns")
    ? static_cast<int>(execution["columns"].size())
    : 0;

  std::stringstream narrative_prompt;
  narrative_prompt << "Summarize this SQL result for a PostgreSQL report: "
                   << pg_llm_write_json(execution);
  auto narrative = model->chat_completion(narrative_prompt.str());
  report["narrative"] = narrative.response;
  report["recommendations"] = Json::arrayValue;
  report["recommendations"].append("Review the generated narrative before sharing externally.");
  if (execution.isMember("explain")) {
    report["recommendations"].append("Inspect the EXPLAIN plan for optimization opportunities.");
  }

  Json::Value vega(Json::objectValue);
  vega["$schema"] = "https://vega.github.io/schema/vega-lite/v5.json";
  if (execution.isMember("columns") && execution["columns"].size() >= 2) {
    vega["mark"] = "bar";
    vega["encoding"]["x"]["field"] = execution["columns"][0];
    vega["encoding"]["x"]["type"] = "nominal";
    vega["encoding"]["y"]["field"] = execution["columns"][1];
    vega["encoding"]["y"]["type"] = "quantitative";
  } else {
    vega["mark"] = "text";
    vega["encoding"]["text"]["field"] = "summary";
    vega["encoding"]["text"]["type"] = "nominal";
  }
  report["vega_lite"] = vega;

  SPI_connect();
  const char* sql_insert =
    "INSERT INTO _pg_llm_catalog.pg_llm_reports "
    "(request_id, instance_name, sql_text, report) VALUES ($1::uuid, $2, $3, $4::jsonb)";
  Oid argtypes[4] = {TEXTOID, TEXTOID, TEXTOID, TEXTOID};
  Datum values[4] = {
    uuid_text_datum(report["request_id"].asString()),
    text_datum(instance_name),
    text_datum(sql),
    text_datum(pg_llm_write_json(report))};
  char nulls[4] = {' ', ' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql_insert, 4, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_INSERT, "failed to persist report");
  SPI_finish();

  insert_audit_log(report["request_id"].asString(),
                   "report",
                   instance_name,
                   "",
                   true,
                   narrative.confidence_score,
                   report);
  insert_trace_log(report["request_id"].asString(), "report", report);
  return report;
}

std::vector<KnowledgeSearchRow> search_knowledge_internal(const std::string& query, int limit) {
  std::vector<float> embedding = deterministic_embedding(query, 64);
  SPI_connect();
  Datum embedding_datum = std_vector_to_vector(embedding);
  const char* sql =
    "SELECT c.id, c.document_id, c.chunk_index, d.source_name, c.content, "
    "CASE WHEN c.content ILIKE $2 THEN 1.0 ELSE 1 - (c.embedding <=> $1) END AS similarity, "
    "COALESCE(c.metadata::text, '{}') "
    "FROM _pg_llm_catalog.pg_llm_knowledge_chunks c "
    "JOIN _pg_llm_catalog.pg_llm_knowledge_documents d ON d.id = c.document_id "
    "ORDER BY CASE WHEN c.content ILIKE $2 THEN 0 ELSE 1 END, c.embedding <=> $1 LIMIT $3";
  Oid argtypes[3] = {get_vector_type_oid(), TEXTOID, INT4OID};
  Datum values[3] = {embedding_datum, text_datum("%" + query + "%"), Int32GetDatum(limit)};
  char nulls[3] = {' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql, 3, argtypes, values, nulls, true, 0);
  ensure_spi_result(ret, SPI_OK_SELECT, "failed to search knowledge");

  std::vector<KnowledgeSearchRow> rows;
  for (uint64 i = 0; i < SPI_processed; ++i) {
    HeapTuple tuple = SPI_tuptable->vals[i];
    TupleDesc tupdesc = SPI_tuptable->tupdesc;
    bool isnull = false;
    KnowledgeSearchRow row;
    row.chunk_id = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isnull));
    row.document_id = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 2, &isnull));
    row.chunk_index = DatumGetInt32(SPI_getbinval(tuple, tupdesc, 3, &isnull));
    row.source_name = SPI_getvalue(tuple, tupdesc, 4);
    row.content = SPI_getvalue(tuple, tupdesc, 5);
    row.similarity = DatumGetFloat4(SPI_getbinval(tuple, tupdesc, 6, &isnull));
    row.metadata_json = SPI_getvalue(tuple, tupdesc, 7);
    rows.push_back(row);
  }

  SPI_finish();
  return rows;
}

std::string build_rag_context(const std::string& query, int limit) {
  auto rows = search_knowledge_internal(query, limit);
  std::stringstream buffer;
  for (const auto& row : rows) {
    buffer << "[" << row.source_name << ":" << row.chunk_index << "] "
           << row.content << "\n";
  }
  return buffer.str();
}

int64 insert_knowledge_document(const std::string& source_name,
                                const std::string& content,
                                const std::string& metadata_json) {
  SPI_connect();
  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_knowledge_documents "
    "(source_name, content, metadata) VALUES ($1, $2, $3::jsonb) RETURNING id";
  Oid argtypes[3] = {TEXTOID, TEXTOID, TEXTOID};
  Datum values[3] = {text_datum(source_name), text_datum(content), text_datum(metadata_json)};
  char nulls[3] = {' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql, 3, argtypes, values, nulls, false, 1);
  ensure_spi_result(ret, SPI_OK_INSERT_RETURNING, "failed to insert knowledge document");

  bool isnull = false;
  int64 document_id = DatumGetInt64(
    SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull));
  SPI_finish();
  return document_id;
}

void insert_knowledge_chunk(int64 document_id,
                            int chunk_index,
                            const std::string& content,
                            const std::string& metadata_json) {
  SPI_connect();
  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_knowledge_chunks "
    "(document_id, chunk_index, content, embedding, metadata) "
    "VALUES ($1, $2, $3, $4, $5::jsonb)";
  Oid argtypes[5] = {INT8OID, INT4OID, TEXTOID, get_vector_type_oid(), TEXTOID};
  Datum values[5] = {
    Int64GetDatum(document_id),
    Int32GetDatum(chunk_index),
    text_datum(content),
    std_vector_to_vector(deterministic_embedding(content, 64)),
    text_datum(metadata_json)};
  char nulls[5] = {' ', ' ', ' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql, 5, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_INSERT, "failed to insert knowledge chunk");
  SPI_finish();
}

std::vector<std::string> split_content(const std::string& content, size_t chunk_size) {
  std::vector<std::string> chunks;
  for (size_t offset = 0; offset < content.size(); offset += chunk_size) {
    chunks.push_back(content.substr(offset, chunk_size));
  }
  if (chunks.empty()) {
    chunks.push_back("");
  }
  return chunks;
}

std::vector<std::string> array_to_strings(ArrayType* array) {
  std::vector<std::string> values;
  Datum* elements = nullptr;
  bool* nulls = nullptr;
  int count = 0;
  int16 typlen = 0;
  bool typbyval = false;
  char typalign = 0;
  get_typlenbyvalalign(ARR_ELEMTYPE(array), &typlen, &typbyval, &typalign);
  deconstruct_array(array,
                    ARR_ELEMTYPE(array),
                    typlen,
                    typbyval,
                    typalign,
                    &elements,
                    &nulls,
                    &count);
  for (int i = 0; i < count; ++i) {
    if (!nulls[i]) {
      text* item = DatumGetTextPP(elements[i]);
      values.push_back(text_to_std_string(item));
    }
  }
  return values;
}

}  // namespace

void _PG_init(void) {
  pg_llm_glog_init_guc();
  pg_llm_define_core_gucs();
  pg_llm_glog_init();
  PG_LLM_LOG_INFO("pg_llm extension loaded");
}

void _PG_fini(void) {
  PG_LLM_LOG_INFO("pg_llm extension unloaded");
  pg_llm_glog_shutdown();
}

Datum pg_llm_add_model(PG_FUNCTION_ARGS) {
  bool local_model = PG_GETARG_BOOL(0);
  std::string model_type = text_to_std_string(PG_GETARG_TEXT_PP(1));
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(2));
  std::string api_key = text_to_std_string(PG_GETARG_TEXT_PP(3));
  std::string config = text_to_std_string(PG_GETARG_TEXT_PP(4));

  Json::Value config_json = pg_llm_parse_json(config);
  PgLlmModelInfo info;
  info.local_model = local_model;
  info.model_type = model_type;
  info.instance_name = instance_name;
  info.confidence_threshold = config_json.get("confidence_threshold", 0.0).asDouble();
  info.fallback_instance = config_json.get("fallback_instance", "").asString();
  info.is_local_fallback = config_json.get("is_local_fallback", false).asBool();
  info.capabilities_json = config_json.isMember("capabilities")
    ? pg_llm_write_json(config_json["capabilities"])
    : "{}";

  if (!api_key.empty() || config_json.isMember("access_key_secret")) {
    info.encrypted_api_key = pg_llm_encrypt_text(api_key);
    info.encrypted_config = pg_llm_encrypt_text(config);
    info.api_key = pg_llm_redact_secret(api_key);
    info.config = "{\"encrypted\":true}";
  } else {
    info.api_key = api_key;
    info.config = config;
  }

  pg_llm_model_insert(info);

  auto& manager = ModelManager::get_instance();
  manager.register_model(model_type, [model_type]() {
    return std::make_unique<LLMInterface>(model_type);
  });

  bool success = manager.create_model_instance(local_model, model_type, instance_name, api_key, config);
  PG_RETURN_BOOL(success);
}

Datum pg_llm_remove_model(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  auto& manager = ModelManager::get_instance();
  bool success = manager.remove_model_instance(instance_name);
  success = pg_llm_model_delete(instance_name) || success;
  PG_RETURN_BOOL(success);
}

Datum pg_llm_chat(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(1));
  auto result = execute_single_chat_internal(instance_name, prompt, Json::Value(Json::objectValue), std::nullopt, false);
  PG_RETURN_TEXT_P(cstring_to_text(result.response.c_str()));
}

Datum pg_llm_parallel_chat(PG_FUNCTION_ARGS) {
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::vector<std::string> model_names = PG_ARGISNULL(1)
    ? pg_llm_get_all_instancenames()
    : array_to_strings(PG_GETARG_ARRAYTYPE_P(1));
  auto result = execute_parallel_chat_internal(prompt, model_names, Json::Value(Json::objectValue));
  PG_RETURN_TEXT_P(cstring_to_text(result.response.c_str()));
}

Datum pg_llm_store_vector(PG_FUNCTION_ARGS) {
  std::string table_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string column_name = text_to_std_string(PG_GETARG_TEXT_PP(1));
  int64 row_id = PG_GETARG_INT64(2);
  Datum vector_datum = PG_GETARG_DATUM(3);
  Jsonb* metadata = PG_ARGISNULL(4) ? nullptr : PG_GETARG_JSONB_P(4);

  SPI_connect();
  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_vectors "
    "(table_name, column_name, row_id, query_vector, metadata) "
    "VALUES ($1, $2, $3, $4, COALESCE($5::jsonb, '{}'::jsonb)) RETURNING id";
  Oid argtypes[5] = {TEXTOID, TEXTOID, INT8OID, get_vector_type_oid(), TEXTOID};
  Datum values[5] = {
    text_datum(table_name),
    text_datum(column_name),
    Int64GetDatum(row_id),
    vector_datum,
    text_datum(metadata ? DatumGetCString(DirectFunctionCall1(jsonb_out, PointerGetDatum(metadata))) : "{}")};
  char nulls[5] = {' ', ' ', ' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql, 5, argtypes, values, nulls, false, 1);
  ensure_spi_result(ret, SPI_OK_INSERT_RETURNING, "failed to store vector");

  bool isnull = false;
  int64 id = DatumGetInt64(SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull));
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
    TupleDesc tupdesc = CreateTemplateTupleDesc(6);
    TupleDescInitEntry(tupdesc, 1, "id", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, 2, "table_name", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 3, "column_name", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 4, "row_id", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, 5, "similarity", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, 6, "metadata", JSONBOID, -1, 0);
    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

    SPI_connect();
    const char* sql =
      "SELECT id, table_name, column_name, row_id, "
      "1 - (query_vector <=> $1) AS similarity, metadata "
      "FROM _pg_llm_catalog.pg_llm_vectors "
      "WHERE 1 - (query_vector <=> $1) >= $2 "
      "ORDER BY query_vector <=> $1 LIMIT $3";
    Oid argtypes[3] = {get_vector_type_oid(), FLOAT4OID, INT4OID};
    Datum values[3] = {
      query_vector_datum,
      Float4GetDatum(similarity_threshold),
      Int32GetDatum(limit_count)};
    char nulls[3] = {' ', ' ', ' '};
    int ret = SPI_execute_with_args(sql, 3, argtypes, values, nulls, true, 0);
    ensure_spi_result(ret, SPI_OK_SELECT, "failed to search vectors");
    funcctx->user_fctx = SPI_tuptable;
    funcctx->max_calls = SPI_processed;
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  SPITupleTable* tuptable = static_cast<SPITupleTable*>(funcctx->user_fctx);
  if (funcctx->call_cntr < funcctx->max_calls) {
    HeapTuple tuple = tuptable->vals[funcctx->call_cntr];
    TupleDesc tupdesc = tuptable->tupdesc;
    Datum values[6];
    bool nulls[6] = {false, false, false, false, false, false};
    for (int i = 0; i < 6; ++i) {
      values[i] = SPI_getbinval(tuple, tupdesc, i + 1, &nulls[i]);
    }
    HeapTuple result = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(result));
  }

  SPI_finish();
  SRF_RETURN_DONE(funcctx);
}

Datum pg_llm_get_embedding(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string input_text = text_to_std_string(PG_GETARG_TEXT_PP(1));
  auto model = get_model_or_error(instance_name);
  PG_RETURN_DATUM(std_vector_to_vector(model->get_embedding(input_text)));
}

Datum pg_llm_text2sql(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(1));
  std::optional<std::string> schema_info;
  if (!PG_ARGISNULL(2)) {
    schema_info = text_to_std_string(PG_GETARG_TEXT_PP(2));
  }
  bool use_vector_search = PG_ARGISNULL(3) ? true : PG_GETARG_BOOL(3);
  Json::Value result = build_text2sql_json_internal(instance_name,
                                                    prompt,
                                                    schema_info,
                                                    use_vector_search,
                                                    Json::Value(Json::objectValue));
  std::string formatted = result["generated_sql"].asString();
  PG_RETURN_TEXT_P(cstring_to_text(formatted.c_str()));
}

Datum pg_llm_create_session(PG_FUNCTION_ARGS) {
  int max_messages = PG_ARGISNULL(0) ? 10 : PG_GETARG_INT32(0);
  std::string session_id = create_session_internal(max_messages);
  PG_RETURN_TEXT_P(cstring_to_text(session_id.c_str()));
}

Datum pg_llm_multi_turn_chat(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string session_id = text_to_std_string(PG_GETARG_TEXT_PP(1));
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(2));
  auto result = execute_single_chat_internal(instance_name, prompt, Json::Value(Json::objectValue), session_id, false);
  PG_RETURN_TEXT_P(cstring_to_text(result.response.c_str()));
}

Datum pg_llm_set_max_messages(PG_FUNCTION_ARGS) {
  std::string session_id = text_to_std_string(PG_GETARG_TEXT_PP(0));
  int max_messages = PG_GETARG_INT32(1);
  get_session_json_internal(session_id, true);
  SPI_connect();
  const char* sql =
    "UPDATE _pg_llm_catalog.pg_llm_sessions SET max_messages = $2 WHERE session_id = $1";
  Oid argtypes[2] = {TEXTOID, INT4OID};
  Datum values[2] = {text_datum(session_id), Int32GetDatum(max_messages)};
  char nulls[2] = {' ', ' '};
  int ret = SPI_execute_with_args(sql, 2, argtypes, values, nulls, false, 0);
  ensure_spi_result(ret, SPI_OK_UPDATE, "failed to set max messages");
  SPI_finish();
  trim_session_messages(session_id, max_messages);
  PG_RETURN_VOID();
}

Datum pg_llm_get_sessions(PG_FUNCTION_ARGS) {
  FuncCallContext* funcctx;
  if (SRF_IS_FIRSTCALL()) {
    funcctx = SRF_FIRSTCALL_INIT();
    MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
    TupleDesc tupdesc = CreateTemplateTupleDesc(4);
    TupleDescInitEntry(tupdesc, 1, "session_id", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 2, "message_count", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, 3, "max_messages", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, 4, "last_active_time", TIMESTAMPTZOID, -1, 0);
    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

    SPI_connect();
    const char* sql =
      "SELECT s.session_id, "
      "COALESCE(m.message_count, 0), s.max_messages, s.last_active_at "
      "FROM _pg_llm_catalog.pg_llm_sessions s "
      "LEFT JOIN ("
      "  SELECT session_id, COUNT(*)::int AS message_count "
      "  FROM _pg_llm_catalog.pg_llm_session_messages GROUP BY session_id"
      ") m ON m.session_id = s.session_id "
      "ORDER BY s.created_at";
    int ret = SPI_execute(sql, true, 0);
    ensure_spi_result(ret, SPI_OK_SELECT, "failed to list sessions");
    funcctx->user_fctx = SPI_tuptable;
    funcctx->max_calls = SPI_processed;
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  SPITupleTable* tuptable = static_cast<SPITupleTable*>(funcctx->user_fctx);
  if (funcctx->call_cntr < funcctx->max_calls) {
    HeapTuple tuple = tuptable->vals[funcctx->call_cntr];
    TupleDesc tupdesc = tuptable->tupdesc;
    Datum values[4];
    bool nulls[4] = {false, false, false, false};
    for (int i = 0; i < 4; ++i) {
      values[i] = SPI_getbinval(tuple, tupdesc, i + 1, &nulls[i]);
    }
    HeapTuple result = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(result));
  }

  SPI_finish();
  SRF_RETURN_DONE(funcctx);
}

Datum pg_llm_cleanup_sessions(PG_FUNCTION_ARGS) {
  cleanup_sessions_internal(PG_GETARG_INT32(0));
  PG_RETURN_VOID();
}

Datum pg_llm_chat_json(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(1));
  Jsonb* options_jsonb = PG_ARGISNULL(2) ? nullptr : PG_GETARG_JSONB_P(2);
  Json::Value options = jsonb_to_value(options_jsonb);
  auto result = execute_single_chat_internal(instance_name, prompt, options, std::nullopt, false);

  Json::Value output(Json::objectValue);
  output["request_id"] = result.request_id;
  output["instance_name"] = result.selected_instance;
  output["model_name"] = result.selected_model_name;
  output["response"] = result.response;
  output["confidence_score"] = result.confidence_score;
  output["fallback_used"] = result.fallback_used;
  output["fallback_instance"] = result.fallback_instance;
  output["candidates"] = result.candidates;
  PG_RETURN_DATUM(json_to_jsonb_datum(output));
}

Datum pg_llm_parallel_chat_json(PG_FUNCTION_ARGS) {
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::vector<std::string> model_names = PG_ARGISNULL(1)
    ? pg_llm_get_all_instancenames()
    : array_to_strings(PG_GETARG_ARRAYTYPE_P(1));
  Jsonb* options_jsonb = PG_ARGISNULL(2) ? nullptr : PG_GETARG_JSONB_P(2);
  Json::Value options = jsonb_to_value(options_jsonb);
  auto result = execute_parallel_chat_internal(prompt, model_names, options);

  Json::Value output(Json::objectValue);
  output["request_id"] = result.request_id;
  output["selected_instance"] = result.selected_instance;
  output["model_name"] = result.selected_model_name;
  output["response"] = result.response;
  output["confidence_score"] = result.confidence_score;
  output["fallback_used"] = result.fallback_used;
  output["fallback_instance"] = result.fallback_instance;
  output["candidates"] = result.candidates;
  PG_RETURN_DATUM(json_to_jsonb_datum(output));
}

Datum pg_llm_text2sql_json(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(1));
  std::optional<std::string> schema_info;
  if (!PG_ARGISNULL(2)) {
    schema_info = text_to_std_string(PG_GETARG_TEXT_PP(2));
  }
  bool use_vector_search = PG_ARGISNULL(3) ? true : PG_GETARG_BOOL(3);
  Jsonb* options_jsonb = PG_ARGISNULL(4) ? nullptr : PG_GETARG_JSONB_P(4);
  Json::Value options = jsonb_to_value(options_jsonb);
  PG_RETURN_DATUM(json_to_jsonb_datum(
    build_text2sql_json_internal(instance_name, prompt, schema_info, use_vector_search, options)));
}

Datum pg_llm_execute_sql_with_analysis(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string sql = text_to_std_string(PG_GETARG_TEXT_PP(1));
  Jsonb* options_jsonb = PG_ARGISNULL(2) ? nullptr : PG_GETARG_JSONB_P(2);
  Json::Value result = build_sql_result_json(sql);
  result["instance_name"] = instance_name;
  result["options"] = jsonb_to_value(options_jsonb);
  result["request_id"] = pg_llm_generate_uuid();
  insert_audit_log(result["request_id"].asString(), "execute_sql", instance_name, "", true, 1.0, result);
  insert_trace_log(result["request_id"].asString(), "execute_sql", result);
  PG_RETURN_DATUM(json_to_jsonb_datum(result));
}

Datum pg_llm_generate_report(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string sql = text_to_std_string(PG_GETARG_TEXT_PP(1));
  Jsonb* options_jsonb = PG_ARGISNULL(2) ? nullptr : PG_GETARG_JSONB_P(2);
  Json::Value options = jsonb_to_value(options_jsonb);
  PG_RETURN_DATUM(json_to_jsonb_datum(build_report_json_internal(instance_name, sql, options)));
}

Datum pg_llm_get_session(PG_FUNCTION_ARGS) {
  std::string session_id = text_to_std_string(PG_GETARG_TEXT_PP(0));
  Json::Value session = get_session_json_internal(session_id, true);
  session["messages"] = get_session_messages_json(session_id);
  PG_RETURN_DATUM(json_to_jsonb_datum(session));
}

Datum pg_llm_get_session_messages(PG_FUNCTION_ARGS) {
  std::string session_id = text_to_std_string(PG_GETARG_TEXT_PP(0));
  FuncCallContext* funcctx;
  if (SRF_IS_FIRSTCALL()) {
    funcctx = SRF_FIRSTCALL_INIT();
    MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
    TupleDesc tupdesc = CreateTemplateTupleDesc(5);
    TupleDescInitEntry(tupdesc, 1, "id", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, 2, "request_id", UUIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, 3, "role", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 4, "content", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 5, "created_at", TIMESTAMPTZOID, -1, 0);
    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

    SPI_connect();
    const char* sql =
      "SELECT id, request_id, role, content, created_at "
      "FROM _pg_llm_catalog.pg_llm_session_messages WHERE session_id = $1 ORDER BY id";
    Oid argtypes[1] = {TEXTOID};
    Datum values[1] = {text_datum(session_id)};
    char nulls[1] = {' '};
    int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, true, 0);
    ensure_spi_result(ret, SPI_OK_SELECT, "failed to fetch session message rows");
    funcctx->user_fctx = SPI_tuptable;
    funcctx->max_calls = SPI_processed;
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  SPITupleTable* tuptable = static_cast<SPITupleTable*>(funcctx->user_fctx);
  if (funcctx->call_cntr < funcctx->max_calls) {
    HeapTuple tuple = tuptable->vals[funcctx->call_cntr];
    TupleDesc tupdesc = tuptable->tupdesc;
    Datum values[5];
    bool nulls[5] = {false, false, false, false, false};
    for (int i = 0; i < 5; ++i) {
      values[i] = SPI_getbinval(tuple, tupdesc, i + 1, &nulls[i]);
    }
    HeapTuple result = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(result));
  }

  SPI_finish();
  SRF_RETURN_DONE(funcctx);
}

Datum pg_llm_update_session_state(PG_FUNCTION_ARGS) {
  std::string session_id = text_to_std_string(PG_GETARG_TEXT_PP(0));
  Json::Value state = jsonb_to_value(PG_GETARG_JSONB_P(1));
  update_session_state_internal(session_id, state);
  PG_RETURN_VOID();
}

Datum pg_llm_delete_session(PG_FUNCTION_ARGS) {
  std::string session_id = text_to_std_string(PG_GETARG_TEXT_PP(0));
  PG_RETURN_BOOL(delete_session_internal(session_id));
}

Datum pg_llm_add_knowledge(PG_FUNCTION_ARGS) {
  std::string source_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string content = text_to_std_string(PG_GETARG_TEXT_PP(1));
  Json::Value metadata = PG_ARGISNULL(2)
    ? Json::Value(Json::objectValue)
    : jsonb_to_value(PG_GETARG_JSONB_P(2));
  Json::Value options = PG_ARGISNULL(3)
    ? Json::Value(Json::objectValue)
    : jsonb_to_value(PG_GETARG_JSONB_P(3));
  int chunk_size = options.get("chunk_size", 200).asInt();

  int64 document_id = insert_knowledge_document(source_name, content, pg_llm_write_json(metadata));
  auto chunks = split_content(content, std::max(chunk_size, 32));
  for (size_t i = 0; i < chunks.size(); ++i) {
    insert_knowledge_chunk(document_id, static_cast<int>(i), chunks[i], pg_llm_write_json(metadata));
  }
  PG_RETURN_INT64(document_id);
}

Datum pg_llm_search_knowledge(PG_FUNCTION_ARGS) {
  std::string query = text_to_std_string(PG_GETARG_TEXT_PP(0));
  Json::Value options = PG_ARGISNULL(1)
    ? Json::Value(Json::objectValue)
    : jsonb_to_value(PG_GETARG_JSONB_P(1));
  int limit = options.get("limit", 5).asInt();

  FuncCallContext* funcctx;
  if (SRF_IS_FIRSTCALL()) {
    funcctx = SRF_FIRSTCALL_INIT();
    MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
    TupleDesc tupdesc = CreateTemplateTupleDesc(7);
    TupleDescInitEntry(tupdesc, 1, "chunk_id", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, 2, "document_id", INT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, 3, "chunk_index", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, 4, "source_name", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 5, "content", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 6, "similarity", FLOAT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, 7, "metadata", JSONBOID, -1, 0);
    funcctx->tuple_desc = BlessTupleDesc(tupdesc);
    funcctx->user_fctx = new std::vector<KnowledgeSearchRow>(search_knowledge_internal(query, limit));
    funcctx->max_calls = static_cast<std::vector<KnowledgeSearchRow>*>(funcctx->user_fctx)->size();
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  auto* rows = static_cast<std::vector<KnowledgeSearchRow>*>(funcctx->user_fctx);
  if (funcctx->call_cntr < funcctx->max_calls) {
    const auto& row = (*rows)[funcctx->call_cntr];
    Datum values[7];
    bool nulls[7] = {false, false, false, false, false, false, false};
    values[0] = Int64GetDatum(row.chunk_id);
    values[1] = Int64GetDatum(row.document_id);
    values[2] = Int32GetDatum(row.chunk_index);
    values[3] = CStringGetTextDatum(row.source_name.c_str());
    values[4] = CStringGetTextDatum(row.content.c_str());
    values[5] = Float4GetDatum(row.similarity);
    values[6] = json_to_jsonb_datum(pg_llm_parse_json(row.metadata_json));
    HeapTuple tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
  }

  delete rows;
  SRF_RETURN_DONE(funcctx);
}

Datum pg_llm_record_feedback(PG_FUNCTION_ARGS) {
  Datum request_id = PG_GETARG_DATUM(0);
  int rating = PG_GETARG_INT32(1);
  std::string feedback = text_to_std_string(PG_GETARG_TEXT_PP(2));
  Json::Value metadata = PG_ARGISNULL(3)
    ? Json::Value(Json::objectValue)
    : jsonb_to_value(PG_GETARG_JSONB_P(3));

  std::string request_id_text = pg_llm_uuid_out_string(request_id);
  SPI_connect();
  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_feedback "
    "(request_id, rating, feedback, metadata) VALUES ($1::uuid, $2, $3, $4::jsonb) RETURNING id";
  Oid argtypes[4] = {TEXTOID, INT4OID, TEXTOID, TEXTOID};
  Datum values[4] = {
    uuid_text_datum(request_id_text),
    Int32GetDatum(rating),
    text_datum(feedback),
    text_datum(pg_llm_write_json(metadata))};
  char nulls[4] = {' ', ' ', ' ', ' '};
  int ret = SPI_execute_with_args(sql, 4, argtypes, values, nulls, false, 1);
  ensure_spi_result(ret, SPI_OK_INSERT_RETURNING, "failed to insert feedback");
  bool isnull = false;
  int64 id = DatumGetInt64(SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull));
  SPI_finish();
  PG_RETURN_INT64(id);
}

Datum pg_llm_get_audit_log(PG_FUNCTION_ARGS) {
  Json::Value options = PG_ARGISNULL(0)
    ? Json::Value(Json::objectValue)
    : jsonb_to_value(PG_GETARG_JSONB_P(0));
  int limit = options.get("limit", 50).asInt();

  FuncCallContext* funcctx;
  if (SRF_IS_FIRSTCALL()) {
    funcctx = SRF_FIRSTCALL_INIT();
    MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
    TupleDesc tupdesc = CreateTemplateTupleDesc(7);
    TupleDescInitEntry(tupdesc, 1, "request_id", UUIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, 2, "event_type", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 3, "instance_name", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 4, "session_id", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 5, "success", BOOLOID, -1, 0);
    TupleDescInitEntry(tupdesc, 6, "confidence_score", FLOAT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, 7, "metadata", JSONBOID, -1, 0);
    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

    SPI_connect();
    const char* sql =
      "SELECT request_id, event_type, instance_name, session_id, success, confidence_score, metadata "
      "FROM _pg_llm_catalog.pg_llm_audit_log ORDER BY created_at DESC LIMIT $1";
    Oid argtypes[1] = {INT4OID};
    Datum values[1] = {Int32GetDatum(limit)};
    char nulls[1] = {' '};
    int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, true, 0);
    ensure_spi_result(ret, SPI_OK_SELECT, "failed to load audit log");
    funcctx->user_fctx = SPI_tuptable;
    funcctx->max_calls = SPI_processed;
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  SPITupleTable* tuptable = static_cast<SPITupleTable*>(funcctx->user_fctx);
  if (funcctx->call_cntr < funcctx->max_calls) {
    HeapTuple tuple = tuptable->vals[funcctx->call_cntr];
    TupleDesc tupdesc = tuptable->tupdesc;
    Datum values[7];
    bool nulls[7] = {false, false, false, false, false, false, false};
    for (int i = 0; i < 7; ++i) {
      values[i] = SPI_getbinval(tuple, tupdesc, i + 1, &nulls[i]);
    }
    HeapTuple result = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(result));
  }

  SPI_finish();
  SRF_RETURN_DONE(funcctx);
}

Datum pg_llm_get_trace(PG_FUNCTION_ARGS) {
  Datum request_id = PG_GETARG_DATUM(0);
  std::string request_id_text = pg_llm_uuid_out_string(request_id);

  SPI_connect();
  const char* sql =
    "SELECT stage, details::text, created_at "
    "FROM _pg_llm_catalog.pg_llm_trace_log WHERE request_id = $1::uuid ORDER BY id";
  Oid argtypes[1] = {TEXTOID};
  Datum values[1] = {uuid_text_datum(request_id_text)};
  char nulls[1] = {' '};
  int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, true, 0);
  ensure_spi_result(ret, SPI_OK_SELECT, "failed to fetch trace log");

  Json::Value result(Json::objectValue);
  result["request_id"] = request_id_text;
  result["events"] = Json::arrayValue;
  for (uint64 i = 0; i < SPI_processed; ++i) {
    Json::Value event(Json::objectValue);
    event["stage"] = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1);
    event["details"] = pg_llm_parse_json(SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2));
    event["created_at"] = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 3);
    result["events"].append(event);
  }
  SPI_finish();

  PG_RETURN_DATUM(json_to_jsonb_datum(result));
}

Datum pg_llm_chat_stream(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(1));
  Json::Value options = PG_ARGISNULL(2)
    ? Json::Value(Json::objectValue)
    : jsonb_to_value(PG_GETARG_JSONB_P(2));

  FuncCallContext* funcctx;
  if (SRF_IS_FIRSTCALL()) {
    funcctx = SRF_FIRSTCALL_INIT();
    MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
    auto model = get_model_or_error(instance_name);
    auto response = model->stream_chat_completion(prompt);
    auto* state = new StreamSrfState();
    state->chunks = response.chunks;
    state->request_id = pg_llm_generate_uuid();
    state->model_name = response.model_name;
    state->confidence_score = response.confidence_score;
    funcctx->user_fctx = state;
    funcctx->max_calls = response.chunks.size();
    TupleDesc tupdesc = CreateTemplateTupleDesc(6);
    TupleDescInitEntry(tupdesc, 1, "seq_no", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, 2, "chunk", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 3, "is_final", BOOLOID, -1, 0);
    TupleDescInitEntry(tupdesc, 4, "model_name", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 5, "confidence_score", FLOAT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, 6, "request_id", UUIDOID, -1, 0);
    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

    Json::Value audit(Json::objectValue);
    audit["prompt"] = prompt;
    audit["streaming"] = true;
    audit["chunk_count"] = static_cast<int>(response.chunks.size());
    insert_audit_log(state->request_id, "chat_stream", instance_name, "", true, response.confidence_score, audit);
    insert_trace_log(state->request_id, "chat_stream", options);
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  auto* state = static_cast<StreamSrfState*>(funcctx->user_fctx);
  if (funcctx->call_cntr < funcctx->max_calls) {
    const auto& chunk = state->chunks[funcctx->call_cntr];
    Datum values[6];
    bool nulls[6] = {false, false, false, false, false, false};
    values[0] = Int32GetDatum(chunk.seq_no);
    values[1] = CStringGetTextDatum(chunk.chunk.c_str());
    values[2] = BoolGetDatum(chunk.is_final);
    values[3] = CStringGetTextDatum(state->model_name.c_str());
    values[4] = Float8GetDatum(state->confidence_score);
    values[5] = pg_llm_uuid_in_datum(state->request_id);
    HeapTuple tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
  }

  delete state;
  SRF_RETURN_DONE(funcctx);
}

Datum pg_llm_multi_turn_chat_stream(PG_FUNCTION_ARGS) {
  std::string instance_name = text_to_std_string(PG_GETARG_TEXT_PP(0));
  std::string session_id = text_to_std_string(PG_GETARG_TEXT_PP(1));
  std::string prompt = text_to_std_string(PG_GETARG_TEXT_PP(2));
  Json::Value options = PG_ARGISNULL(3)
    ? Json::Value(Json::objectValue)
    : jsonb_to_value(PG_GETARG_JSONB_P(3));

  FuncCallContext* funcctx;
  if (SRF_IS_FIRSTCALL()) {
    funcctx = SRF_FIRSTCALL_INIT();
    MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
    auto model = get_model_or_error(instance_name);
    auto response = model->stream_chat_completion(load_session_messages(session_id));
    auto* state = new StreamSrfState();
    state->chunks = response.chunks;
    state->request_id = pg_llm_generate_uuid();
    state->model_name = response.model_name;
    state->confidence_score = response.confidence_score;
    funcctx->user_fctx = state;
    funcctx->max_calls = response.chunks.size();
    TupleDesc tupdesc = CreateTemplateTupleDesc(6);
    TupleDescInitEntry(tupdesc, 1, "seq_no", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, 2, "chunk", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 3, "is_final", BOOLOID, -1, 0);
    TupleDescInitEntry(tupdesc, 4, "model_name", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, 5, "confidence_score", FLOAT8OID, -1, 0);
    TupleDescInitEntry(tupdesc, 6, "request_id", UUIDOID, -1, 0);
    funcctx->tuple_desc = BlessTupleDesc(tupdesc);
    insert_trace_log(state->request_id, "multi_turn_chat_stream", options);
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  auto* state = static_cast<StreamSrfState*>(funcctx->user_fctx);
  if (funcctx->call_cntr < funcctx->max_calls) {
    const auto& chunk = state->chunks[funcctx->call_cntr];
    Datum values[6];
    bool nulls[6] = {false, false, false, false, false, false};
    values[0] = Int32GetDatum(chunk.seq_no);
    values[1] = CStringGetTextDatum(chunk.chunk.c_str());
    values[2] = BoolGetDatum(chunk.is_final);
    values[3] = CStringGetTextDatum(state->model_name.c_str());
    values[4] = Float8GetDatum(state->confidence_score);
    values[5] = pg_llm_uuid_in_datum(state->request_id);
    HeapTuple tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
  }

  delete state;
  SRF_RETURN_DONE(funcctx);
}
