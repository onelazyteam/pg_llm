#include "catalog/pg_llm_models.h"

extern "C" {
#include "executor/spi.h"
#include "utils/builtins.h"
}

namespace {

void ensure_spi_ok(int code, int expected, const char* message) {
  if (code != expected) {
    ereport(ERROR,
            (errcode(ERRCODE_INTERNAL_ERROR),
             errmsg("%s: %s", message, SPI_result_code_string(code))));
  }
}

Datum text_datum(const std::string& value) {
  return CStringGetTextDatum(value.c_str());
}

}  // namespace

void pg_llm_model_insert(const PgLlmModelInfo& info) {
  SPI_connect();

  const char* sql =
    "INSERT INTO _pg_llm_catalog.pg_llm_models ("
    "local_model, model_type, instance_name, api_key, config, "
    "encrypted_api_key, encrypted_config, confidence_threshold, "
    "fallback_instance, is_local_fallback, capabilities) "
    "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11::jsonb) "
    "ON CONFLICT (instance_name) DO UPDATE SET "
    "local_model = EXCLUDED.local_model, "
    "model_type = EXCLUDED.model_type, "
    "api_key = EXCLUDED.api_key, "
    "config = EXCLUDED.config, "
    "encrypted_api_key = EXCLUDED.encrypted_api_key, "
    "encrypted_config = EXCLUDED.encrypted_config, "
    "confidence_threshold = EXCLUDED.confidence_threshold, "
    "fallback_instance = EXCLUDED.fallback_instance, "
    "is_local_fallback = EXCLUDED.is_local_fallback, "
    "capabilities = EXCLUDED.capabilities, "
    "updated_at = CURRENT_TIMESTAMP";

  Oid argtypes[11] = {
    BOOLOID, TEXTOID, TEXTOID, TEXTOID, TEXTOID, TEXTOID, TEXTOID, FLOAT8OID, TEXTOID, BOOLOID, TEXTOID};
  Datum values[11] = {
    BoolGetDatum(info.local_model),
    text_datum(info.model_type),
    text_datum(info.instance_name),
    text_datum(info.api_key),
    text_datum(info.config),
    text_datum(info.encrypted_api_key),
    text_datum(info.encrypted_config),
    Float8GetDatum(info.confidence_threshold),
    text_datum(info.fallback_instance),
    BoolGetDatum(info.is_local_fallback),
    text_datum(info.capabilities_json.empty() ? "{}" : info.capabilities_json)};
  char nulls[11] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};

  int ret = SPI_execute_with_args(sql, 11, argtypes, values, nulls, false, 0);
  ensure_spi_ok(ret, SPI_OK_INSERT, "failed to upsert model metadata");
  SPI_finish();
}

bool pg_llm_model_delete(const std::string& instance_name) {
  SPI_connect();
  const char* sql = "DELETE FROM _pg_llm_catalog.pg_llm_models WHERE instance_name = $1";
  Oid argtypes[1] = {TEXTOID};
  Datum values[1] = {text_datum(instance_name)};
  char nulls[1] = {' '};
  int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, false, 0);
  ensure_spi_ok(ret, SPI_OK_DELETE, "failed to delete model metadata");
  uint64 affected = SPI_processed;
  SPI_finish();
  return affected > 0;
}

bool pg_llm_model_get_info(const std::string& instance_name, PgLlmModelInfo* info) {
  SPI_connect();
  const char* sql =
    "SELECT local_model, model_type, instance_name, "
    "COALESCE(api_key, ''), COALESCE(config, ''), "
    "COALESCE(encrypted_api_key, ''), COALESCE(encrypted_config, ''), "
    "COALESCE(confidence_threshold, 0.0), COALESCE(fallback_instance, ''), "
    "COALESCE(is_local_fallback, false), COALESCE(capabilities::text, '{}') "
    "FROM _pg_llm_catalog.pg_llm_models WHERE instance_name = $1";

  Oid argtypes[1] = {TEXTOID};
  Datum values[1] = {text_datum(instance_name)};
  char nulls[1] = {' '};
  int ret = SPI_execute_with_args(sql, 1, argtypes, values, nulls, true, 1);
  ensure_spi_ok(ret, SPI_OK_SELECT, "failed to fetch model metadata");
  if (SPI_processed == 0) {
    SPI_finish();
    return false;
  }

  bool isnull = false;
  HeapTuple tuple = SPI_tuptable->vals[0];
  TupleDesc tupdesc = SPI_tuptable->tupdesc;
  info->local_model = DatumGetBool(SPI_getbinval(tuple, tupdesc, 1, &isnull));
  info->model_type = SPI_getvalue(tuple, tupdesc, 2);
  info->instance_name = SPI_getvalue(tuple, tupdesc, 3);
  info->api_key = SPI_getvalue(tuple, tupdesc, 4);
  info->config = SPI_getvalue(tuple, tupdesc, 5);
  info->encrypted_api_key = SPI_getvalue(tuple, tupdesc, 6);
  info->encrypted_config = SPI_getvalue(tuple, tupdesc, 7);
  info->confidence_threshold = DatumGetFloat8(SPI_getbinval(tuple, tupdesc, 8, &isnull));
  info->fallback_instance = SPI_getvalue(tuple, tupdesc, 9);
  info->is_local_fallback = DatumGetBool(SPI_getbinval(tuple, tupdesc, 10, &isnull));
  info->capabilities_json = SPI_getvalue(tuple, tupdesc, 11);
  SPI_finish();
  return true;
}

bool pg_llm_model_get_infos(bool* local_model,
                            const std::string instance_name,
                            std::string& model_type,
                            std::string& api_key,
                            std::string& config) {
  PgLlmModelInfo info;
  if (!pg_llm_model_get_info(instance_name, &info)) {
    return false;
  }

  *local_model = info.local_model;
  model_type = info.model_type;
  api_key = info.api_key;
  config = info.config;
  return true;
}

std::vector<std::string> pg_llm_get_all_instancenames() {
  SPI_connect();
  const char* sql =
    "SELECT instance_name FROM _pg_llm_catalog.pg_llm_models ORDER BY instance_name";
  int ret = SPI_execute(sql, true, 0);
  ensure_spi_ok(ret, SPI_OK_SELECT, "failed to list model instances");

  std::vector<std::string> names;
  for (uint64 i = 0; i < SPI_processed; ++i) {
    names.emplace_back(SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1));
  }

  SPI_finish();
  return names;
}
