#ifndef CONVERSATION_H
#define CONVERSATION_H

#include "postgres.h"

// Initialize conversation context management
void initialize_conversation_context(void);

// Clean up conversation context management
void cleanup_conversation_context(void);

// Handle conversation message
char* handle_conversation(const char *message, int conversation_id);

// Create new conversation
int create_conversation(void);

// Delete conversation
bool delete_conversation(int conversation_id);

// Get conversation history
char* get_conversation_history(int conversation_id);

#endif /* CONVERSATION_H */ 