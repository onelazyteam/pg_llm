#include "models/llm_interface.h"

namespace pg_llm {

// Streaming callback function (processes data chunk by chunk)
size_t LLMInterface::stream_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t realsize = size * nmemb;
  StreamContext* ctx = static_cast<StreamContext*>(userp);
  ctx->buffer.append(static_cast<char*>(contents), realsize);

  // Process data line by line
  size_t pos = 0;
  while ((pos = ctx->buffer.find("\n")) != std::string::npos) {
    std::string line = ctx->buffer.substr(0, pos);
    ctx->buffer.erase(0, pos + 1);

    // Skip empty lines and "[DONE]"
    if (line.empty() || line == "data: [DONE]") continue;

    // Extract valid JSON data
    if (line.find("data: ") == 0) { // Check prefix
      line = line.substr(6); // Remove "data: " prefix

      // Parse JSON using jsoncpp
      Json::Value chunk;
      Json::CharReaderBuilder readerBuilder;
      std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
      const char* lineStart = line.c_str();
      std::string parseErrors;

      if (reader->parse(lineStart, lineStart + line.size(), &chunk, &parseErrors)) {
        // Check if choices array exists and is not empty
        if (chunk.isMember("choices") && chunk["choices"].isArray() && !chunk["choices"].empty()) {
          Json::Value& delta = chunk["choices"][0u]["delta"]; // Use unsigned index

          // Check if delta contains content field
          if (delta.isMember("content") && delta["content"].isString()) {
            std::string content = delta["content"].asString();
            if (!content.empty()) {
              ctx->fullReply += content;
              std::cout << content << std::flush; // Real-time output
            }
          }
        }
      } else {
        std::cerr << "JSON parsing error: " << parseErrors << std::endl;
      }
    }
  }

  return realsize;
}

std::string LLMInterface::generate_signature(const std::string& request_body) {
  // Generate timestamp
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                     now.time_since_epoch()).count();

  // Prepare string to sign
  std::stringstream string_to_sign;
  string_to_sign << "POST\n"
                 << api_endpoint_ << "\n"
                 << timestamp << "\n"
                 << request_body;

  // Generate HMAC-SHA256 signature
  unsigned char* hmac = HMAC(EVP_sha256(),
                             access_key_secret_.c_str(), access_key_secret_.length(),
                             (unsigned char*)string_to_sign.str().c_str(),
                             string_to_sign.str().length(),
                             NULL, NULL);

  // Convert to Base64
  BIO* b64 = BIO_new(BIO_f_base64());
  BIO* bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
  BIO_write(b64, hmac, 32);
  BIO_flush(b64);

  BUF_MEM* bptr;
  BIO_get_mem_ptr(b64, &bptr);

  std::string signature(bptr->data, bptr->length);
  BIO_free_all(b64);

  return signature;
}

} // namespace pg_llm