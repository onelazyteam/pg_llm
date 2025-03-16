#pragma once

#include "models/llm_interface.h"

#include <chrono>

namespace pg_llm {

// Structure to hold audit log entry
struct AuditLogEntry {
    std::string session_id;
    std::string user_id;
    std::string model_name;
    std::string prompt;
    std::string response;
    std::chrono::system_clock::time_point timestamp;
    std::string metadata;
};

class ChatSession {
public:
    ChatSession(const std::string& session_id, 
                const std::string& user_id,
                std::shared_ptr<LLMInterface> model);

    // Add a message to the conversation
    void add_message(const std::string& role, const std::string& content);

    // Get the full conversation history
    const std::vector<ChatMessage>& get_messages() const;

    // Get a response from the model
    ModelResponse get_response(const std::string& prompt);

    // Get session ID
    std::string get_session_id() const;

    // Get user ID
    std::string get_user_id() const;

    // Get associated model
    std::shared_ptr<LLMInterface> get_model() const;

    // Add audit log entry
    void add_audit_log(const std::string& prompt, 
                      const std::string& response,
                      const std::string& metadata = "");

    // Get audit logs
    const std::vector<AuditLogEntry>& get_audit_logs() const;

private:
    std::string session_id_;
    std::string user_id_;
    std::shared_ptr<LLMInterface> model_;
    std::vector<ChatMessage> messages_;
    std::vector<AuditLogEntry> audit_logs_;
};

} // namespace pg_llm
