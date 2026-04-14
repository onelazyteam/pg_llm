#pragma once

extern "C" {
#include "postgres.h"
#include "fmgr.h"
#include "utils/jsonb.h"
}

#include <string>

#include <json/json.h>

extern char* pg_llm_master_key;
extern bool pg_llm_audit_enabled;
extern bool pg_llm_trace_enabled;
extern bool pg_llm_redact_sensitive;
extern double pg_llm_audit_sample_rate;
extern double pg_llm_default_confidence_threshold;
extern char* pg_llm_default_local_fallback;

void pg_llm_define_core_gucs(void);

std::string pg_llm_generate_uuid();
Datum pg_llm_uuid_in_datum(const std::string& uuid_str);
std::string pg_llm_uuid_out_string(Datum uuid_datum);

Json::Value pg_llm_parse_json(const std::string& input);
std::string pg_llm_write_json(const Json::Value& value);
Datum pg_llm_jsonb_from_string(const std::string& input);

std::string pg_llm_encrypt_text(const std::string& plain_text);
std::string pg_llm_decrypt_text(const std::string& cipher_text);
std::string pg_llm_redact_secret(const std::string& input);
