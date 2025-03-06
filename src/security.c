#include "../include/security.h"
#include <string.h>
#include <json-c/json.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

// Security configuration
typedef struct {
    char **banned_keywords;
    int banned_keyword_count;
    char **sensitive_patterns;
    int sensitive_pattern_count;
    char *encryption_key;
    bool audit_enabled;
} SecurityConfig;

static SecurityConfig *config = NULL;

// Initialize security module
void initialize_security(void)
{
    config = palloc(sizeof(SecurityConfig));
    memset(config, 0, sizeof(SecurityConfig));
    
    // Default configuration
    config->banned_keywords = palloc(sizeof(char*) * 10);
    config->banned_keyword_count = 3;
    config->banned_keywords[0] = pstrdup("DROP DATABASE");
    config->banned_keywords[1] = pstrdup("TRUNCATE");
    config->banned_keywords[2] = pstrdup("ALTER SYSTEM");
    
    config->sensitive_patterns = palloc(sizeof(char*) * 10);
    config->sensitive_pattern_count = 2;
    config->sensitive_patterns[0] = pstrdup("\\d{16}"); // Credit card format
    config->sensitive_patterns[1] = pstrdup("\\d{3}-\\d{2}-\\d{4}"); // US Social Security Number
    
    // Generate random encryption key
    config->encryption_key = palloc(33);
    RAND_bytes((unsigned char*)config->encryption_key, 32);
    config->encryption_key[32] = '\0';
    
    config->audit_enabled = true;
}

// Clean up security module
void cleanup_security(void)
{
    if (config) {
        for (int i = 0; i < config->banned_keyword_count; i++) {
            pfree(config->banned_keywords[i]);
        }
        pfree(config->banned_keywords);
        
        for (int i = 0; i < config->sensitive_pattern_count; i++) {
            pfree(config->sensitive_patterns[i]);
        }
        pfree(config->sensitive_patterns);
        
        pfree(config->encryption_key);
        pfree(config);
    }
}

// Security check function
bool security_check(const char *query)
{
    if (!config || !query) {
        return false;
    }
    
    for (int i = 0; i < config->banned_keyword_count; i++) {
        if (strstr(query, config->banned_keywords[i]) != NULL) {
            elog(WARNING, "Security check: Query contains banned keyword %s", config->banned_keywords[i]);
            return false;
        }
    }
    
    return true;
}

// Audit record function
void log_audit_record(const char *query, const char *result)
{
    if (!config->audit_enabled) {
        return;
    }
    
    // Actual audit records will be written to database table
    // Simplified here to log output
    elog(NOTICE, "Audit record: Query '%s'", query);
}

// Encrypt sensitive information
char* encrypt_sensitive_info(const char *text)
{
    if (!text) {
        return NULL;
    }
    
    // Simplified implementation, should use appropriate encryption algorithm
    char *encrypted = palloc(strlen(text) * 2 + 1);
    
    // Simple XOR encryption example
    for (int i = 0; text[i]; i++) {
        sprintf(encrypted + i*2, "%02x", text[i] ^ config->encryption_key[i % 32]);
    }
    
    return encrypted;
}

// Decrypt sensitive information
char* decrypt_sensitive_info(const char *encrypted_text)
{
    if (!encrypted_text) {
        return NULL;
    }
    
    int len = strlen(encrypted_text) / 2;
    char *decrypted = palloc(len + 1);
    unsigned int byte;
    
    for (int i = 0; i < len; i++) {
        sscanf(encrypted_text + i*2, "%2x", &byte);
        decrypted[i] = byte ^ config->encryption_key[i % 32];
    }
    decrypted[len] = '\0';
    
    return decrypted;
} 