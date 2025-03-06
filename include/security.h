#ifndef SECURITY_H
#define SECURITY_H

#include "postgres.h"

// Initialize security module
void initialize_security(void);

// Clean up security module
void cleanup_security(void);

// Security check function
bool security_check(const char *query);

// Audit record function
void log_audit_record(const char *query, const char *result);

// Encrypt sensitive information
char* encrypt_sensitive_info(const char *text);

// Decrypt sensitive information
char* decrypt_sensitive_info(const char *encrypted_text);

// Load security rules
bool load_security_rules(const char *rules_json);

#endif /* SECURITY_H */ 