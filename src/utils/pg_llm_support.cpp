#include "utils/pg_llm_support.h"

extern "C" {
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/uuid.h"
}

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <array>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

char* pg_llm_master_key = nullptr;
bool pg_llm_audit_enabled = true;
bool pg_llm_trace_enabled = true;
bool pg_llm_redact_sensitive = true;
double pg_llm_audit_sample_rate = 1.0;
double pg_llm_default_confidence_threshold = 0.60;
char* pg_llm_default_local_fallback = nullptr;

void pg_llm_define_core_gucs(void) {
  DefineCustomStringVariable("pg_llm.master_key",
                             "Master key used to encrypt pg_llm secrets.",
                             nullptr,
                             &pg_llm_master_key,
                             nullptr,
                             PGC_SUSET,
                             GUC_SUPERUSER_ONLY,
                             nullptr,
                             nullptr,
                             nullptr);

  DefineCustomBoolVariable("pg_llm.audit_enabled",
                           "Enable pg_llm audit event persistence.",
                           nullptr,
                           &pg_llm_audit_enabled,
                           true,
                           PGC_SUSET,
                           0,
                           nullptr,
                           nullptr,
                           nullptr);

  DefineCustomBoolVariable("pg_llm.trace_enabled",
                           "Enable pg_llm trace event persistence.",
                           nullptr,
                           &pg_llm_trace_enabled,
                           true,
                           PGC_SUSET,
                           0,
                           nullptr,
                           nullptr,
                           nullptr);

  DefineCustomBoolVariable("pg_llm.redact_sensitive",
                           "Redact sensitive fields in audit and trace output.",
                           nullptr,
                           &pg_llm_redact_sensitive,
                           true,
                           PGC_SUSET,
                           0,
                           nullptr,
                           nullptr,
                           nullptr);

  DefineCustomRealVariable("pg_llm.audit_sample_rate",
                           "Sampling rate for audit log persistence.",
                           nullptr,
                           &pg_llm_audit_sample_rate,
                           1.0,
                           0.0,
                           1.0,
                           PGC_SUSET,
                           0,
                           nullptr,
                           nullptr,
                           nullptr);

  DefineCustomRealVariable("pg_llm.default_confidence_threshold",
                           "Default threshold used to trigger local fallback.",
                           nullptr,
                           &pg_llm_default_confidence_threshold,
                           0.60,
                           0.0,
                           1.0,
                           PGC_SUSET,
                           0,
                           nullptr,
                           nullptr,
                           nullptr);

  DefineCustomStringVariable("pg_llm.default_local_fallback",
                             "Default local fallback model instance.",
                             nullptr,
                             &pg_llm_default_local_fallback,
                             nullptr,
                             PGC_SUSET,
                             0,
                             nullptr,
                             nullptr,
                             nullptr);
}

std::string pg_llm_generate_uuid() {
  std::array<unsigned char, 16> bytes{};
  std::random_device rd;
  for (auto& value : bytes) {
    value = static_cast<unsigned char>(rd());
  }

  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

  char buffer[37];
  snprintf(buffer,
           sizeof(buffer),
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           bytes[0],
           bytes[1],
           bytes[2],
           bytes[3],
           bytes[4],
           bytes[5],
           bytes[6],
           bytes[7],
           bytes[8],
           bytes[9],
           bytes[10],
           bytes[11],
           bytes[12],
           bytes[13],
           bytes[14],
           bytes[15]);
  return buffer;
}

Datum pg_llm_uuid_in_datum(const std::string& uuid_str) {
  return DirectFunctionCall1(uuid_in, CStringGetDatum(uuid_str.c_str()));
}

std::string pg_llm_uuid_out_string(Datum uuid_datum) {
  Datum value = DirectFunctionCall1(uuid_out, uuid_datum);
  return DatumGetCString(value);
}

Json::Value pg_llm_parse_json(const std::string& input) {
  Json::Value value;
  if (input.empty()) {
    return value;
  }

  Json::CharReaderBuilder builder;
  std::string errors;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  if (!reader->parse(input.data(), input.data() + input.size(), &value, &errors)) {
    throw std::runtime_error("failed to parse JSON: " + errors);
  }

  return value;
}

std::string pg_llm_write_json(const Json::Value& value) {
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  return Json::writeString(builder, value);
}

Datum pg_llm_jsonb_from_string(const std::string& input) {
  return DirectFunctionCall1(jsonb_in, CStringGetDatum(input.c_str()));
}

static std::vector<unsigned char> derive_key() {
  if (pg_llm_master_key == nullptr || pg_llm_master_key[0] == '\0') {
    throw std::runtime_error("pg_llm.master_key is not set");
  }

  std::vector<unsigned char> key(SHA256_DIGEST_LENGTH);
  SHA256(reinterpret_cast<const unsigned char*>(pg_llm_master_key),
         strlen(pg_llm_master_key),
         key.data());
  return key;
}

static std::string base64_encode(const unsigned char* data, int length) {
  int encoded_length = 4 * ((length + 2) / 3);
  std::string output(encoded_length, '\0');
  EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&output[0]), data, length);
  return output;
}

static std::vector<unsigned char> base64_decode(const std::string& input) {
  std::vector<unsigned char> output(input.size(), 0);
  int decoded = EVP_DecodeBlock(output.data(),
                                reinterpret_cast<const unsigned char*>(input.data()),
                                input.size());
  if (decoded < 0) {
    throw std::runtime_error("invalid base64 ciphertext");
  }
  while (!output.empty() && output.back() == 0) {
    output.pop_back();
  }
  return output;
}

std::string pg_llm_encrypt_text(const std::string& plain_text) {
  auto key = derive_key();
  std::array<unsigned char, 12> iv{};
  if (RAND_bytes(iv.data(), iv.size()) != 1) {
    throw std::runtime_error("failed to generate IV");
  }

  std::vector<unsigned char> cipher(plain_text.size() + 16);
  std::array<unsigned char, 16> tag{};
  int len = 0;
  int cipher_len = 0;

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    throw std::runtime_error("failed to allocate cipher context");
  }

  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1 ||
      EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1 ||
      EVP_EncryptUpdate(ctx,
                        cipher.data(),
                        &len,
                        reinterpret_cast<const unsigned char*>(plain_text.data()),
                        plain_text.size()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("failed to encrypt text");
  }

  cipher_len = len;
  if (EVP_EncryptFinal_ex(ctx, cipher.data() + cipher_len, &len) != 1 ||
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("failed to finalize encryption");
  }

  cipher_len += len;
  EVP_CIPHER_CTX_free(ctx);

  std::vector<unsigned char> payload;
  payload.insert(payload.end(), iv.begin(), iv.end());
  payload.insert(payload.end(), tag.begin(), tag.end());
  payload.insert(payload.end(), cipher.begin(), cipher.begin() + cipher_len);
  return base64_encode(payload.data(), payload.size());
}

std::string pg_llm_decrypt_text(const std::string& cipher_text) {
  auto payload = base64_decode(cipher_text);
  if (payload.size() < 28) {
    throw std::runtime_error("ciphertext payload is too short");
  }

  auto key = derive_key();
  const unsigned char* iv = payload.data();
  const unsigned char* tag = payload.data() + 12;
  const unsigned char* cipher = payload.data() + 28;
  int cipher_len = payload.size() - 28;

  std::vector<unsigned char> plain(cipher_len + 1, 0);
  int len = 0;
  int plain_len = 0;

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    throw std::runtime_error("failed to allocate cipher context");
  }

  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
      EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) != 1 ||
      EVP_DecryptUpdate(ctx, plain.data(), &len, cipher, cipher_len) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("failed to decrypt text");
  }

  plain_len = len;
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<unsigned char*>(tag)) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("failed to set decryption tag");
  }

  if (EVP_DecryptFinal_ex(ctx, plain.data() + plain_len, &len) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("failed to authenticate ciphertext");
  }

  plain_len += len;
  EVP_CIPHER_CTX_free(ctx);
  return std::string(reinterpret_cast<char*>(plain.data()), plain_len);
}

std::string pg_llm_redact_secret(const std::string& input) {
  if (!pg_llm_redact_sensitive || input.empty()) {
    return input;
  }

  if (input.size() <= 4) {
    return "****";
  }

  return input.substr(0, 2) + "****" + input.substr(input.size() - 2);
}
