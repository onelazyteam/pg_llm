#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <ctime>

#include "models/llm_interface.h"

namespace pg_llm {

// Structure to hold chat session information
struct ChatSession {
  std::string session_id;
  std::vector<ChatMessage> messages;
  time_t last_active_time;
  int max_messages;  // Maximum number of messages allowed
};

// Singleton class for managing chat sessions
class SessionManager {
public:
  static SessionManager& get_instance() {
    static SessionManager instance;
    return instance;
  }

  // Create a new chat session
  std::string create_session(int max_messages = 10) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string session_id = generate_session_id();
    sessions_[session_id] = ChatSession{session_id, {}, time(nullptr), max_messages};
    return session_id;
  }

  // Get an existing session by ID
  ChatSession* get_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
      it->second.last_active_time = time(nullptr);
      return &(it->second);
    }
    return nullptr;
  }

  // Add a message to an existing session
  void add_message(const std::string& session_id, const ChatMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
      it->second.messages.push_back(message);
      // Remove oldest messages if exceeding max_messages
      while ((int)it->second.messages.size() > it->second.max_messages) {
        it->second.messages.erase(it->second.messages.begin());
      }
      it->second.last_active_time = time(nullptr);
    }
  }

  // Set maximum number of messages for a session
  bool set_max_messages(const std::string& session_id, int max_messages) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
      it->second.max_messages = max_messages;
      // Remove oldest messages if exceeding new max_messages
      while ((int)it->second.messages.size() > max_messages) {
        it->second.messages.erase(it->second.messages.begin());
      }
      return true;
    }
    return false;
  }

  // Get all sessions information
  std::vector<ChatSession> get_all_sessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ChatSession> result;
    for (const auto& pair : sessions_) {
      result.push_back(pair.second);
    }
    return result;
  }

  // Clean up expired sessions
  void cleanup_expired_sessions(int timeout_seconds = 3600) {
    std::lock_guard<std::mutex> lock(mutex_);
    time_t now = time(nullptr);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
      if (now - it->second.last_active_time > timeout_seconds) {
        it = sessions_.erase(it);
      } else {
        ++it;
      }
    }
  }

private:
  SessionManager() = default;
  ~SessionManager() = default;
  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;

  // Generate a unique session ID
  std::string generate_session_id() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string result;
    result.reserve(32);
    for (int i = 0; i < 32; ++i) {
      result += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return result;
  }

  std::map<std::string, ChatSession> sessions_;
  std::mutex mutex_;
};

} // namespace pg_llm 