#include "models/chat_session.h"

namespace pg_llm {

ChatSession::ChatSession(const std::string& session_id,
                        const std::string& user_id,
                        std::shared_ptr<LLMInterface> model)
    : session_id_(session_id), user_id_(user_id), model_(model) {}

void ChatSession::add_message(const std::string& role, const std::string& content) {
    messages_.push_back({role, content});
}

const std::vector<ChatMessage>& ChatSession::get_messages() const {
    return messages_;
}

ModelResponse ChatSession::get_response(const std::string& prompt) {
    // Add user message to history
    add_message("user", prompt);

    // Get response from model
    ModelResponse response = model_->chat_completion(messages_);

    // Add assistant response to history
    add_message("assistant", response.response);

    // Add audit log
    add_audit_log(prompt, response.response);

    return response;
}

std::string ChatSession::get_session_id() const {
    return session_id_;
}

std::string ChatSession::get_user_id() const {
    return user_id_;
}

std::shared_ptr<LLMInterface> ChatSession::get_model() const {
    return model_;
}

void ChatSession::add_audit_log(const std::string& prompt,
                               const std::string& response,
                               const std::string& metadata) {
    AuditLogEntry log{
        session_id_,
        user_id_,
        model_->get_model_name(),
        prompt,
        response,
        std::chrono::system_clock::now(),
        metadata
    };
    audit_logs_.push_back(log);
}

const std::vector<AuditLogEntry>& ChatSession::get_audit_logs() const {
    return audit_logs_;
}

} // namespace pg_llm 