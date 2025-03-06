# pg_llm Storage Layer Design

## 1. Configuration Management

### 1.1 Configuration Storage
```sql
-- Configuration table
CREATE TABLE pg_llm_config (
    key text PRIMARY KEY,
    value jsonb NOT NULL,
    created_at timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Configuration history table
CREATE TABLE pg_llm_config_history (
    id serial PRIMARY KEY,
    key text NOT NULL,
    value jsonb NOT NULL,
    changed_at timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
    changed_by text NOT NULL
);
```

### 1.2 Configuration Access
```c
typedef struct ConfigAccess {
    char *key;
    Jsonb *value;
    time_t timestamp;
    bool cached;
} ConfigAccess;

// Configuration operations
bool save_config(const char *key, Jsonb *value);
Jsonb *load_config(const char *key);
bool delete_config(const char *key);
```

### 1.3 Configuration Cache
```c
typedef struct ConfigCache {
    char *key;
    Jsonb *value;
    time_t expires_at;
    bool valid;
} ConfigCache;

// Cache operations
void cache_config(ConfigCache *cache);
Jsonb *get_cached_config(const char *key);
void invalidate_config_cache(const char *key);
```

## 2. Session Management

### 2.1 Session Storage
```sql
-- Session table
CREATE TABLE pg_llm_sessions (
    id serial PRIMARY KEY,
    user_id text NOT NULL,
    created_at timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_active timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
    metadata jsonb
);

-- Session history table
CREATE TABLE pg_llm_session_history (
    id serial PRIMARY KEY,
    session_id integer REFERENCES pg_llm_sessions(id),
    message text NOT NULL,
    role text NOT NULL,
    created_at timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

### 2.2 Session Access
```c
typedef struct SessionAccess {
    int session_id;
    char *user_id;
    time_t created_at;
    time_t last_active;
    Jsonb *metadata;
} SessionAccess;

// Session operations
int create_session(const char *user_id);
bool update_session(int session_id, Jsonb *metadata);
bool delete_session(int session_id);
```

### 2.3 Session Cache
```c
typedef struct SessionCache {
    int session_id;
    char **history;
    int history_count;
    time_t expires_at;
} SessionCache;

// Cache operations
void cache_session(SessionCache *cache);
char **get_cached_history(int session_id);
void invalidate_session_cache(int session_id);
```

## 3. Cache System

### 3.1 Cache Structure
```c
typedef struct CacheEntry {
    char *key;
    void *data;
    size_t size;
    time_t created_at;
    time_t expires_at;
    bool compressed;
} CacheEntry;

typedef struct CacheStats {
    int hit_count;
    int miss_count;
    double hit_ratio;
    size_t memory_used;
} CacheStats;
```

### 3.2 Cache Operations
```c
// Basic operations
bool cache_set(const char *key, void *data, size_t size);
void *cache_get(const char *key, size_t *size);
bool cache_delete(const char *key);
void cache_clear(void);

// Advanced operations
bool cache_set_ex(const char *key, void *data, size_t size, int ttl);
bool cache_set_nx(const char *key, void *data, size_t size);
int cache_ttl(const char *key);
```

### 3.3 Cache Strategy
```c
typedef struct CachePolicy {
    int max_size;
    int max_entries;
    int default_ttl;
    bool use_compression;
    float eviction_threshold;
} CachePolicy;

// Strategy operations
void set_cache_policy(CachePolicy *policy);
void apply_eviction_policy(void);
void optimize_cache_usage(void);
```

## 4. Audit Log

### 4.1 Log Storage
```sql
-- Audit log table
CREATE TABLE pg_llm_audit_log (
    id serial PRIMARY KEY,
    timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
    user_id text NOT NULL,
    action text NOT NULL,
    object_type text NOT NULL,
    object_id text NOT NULL,
    details jsonb,
    client_info jsonb
);

-- Sensitive operation log
CREATE TABLE pg_llm_sensitive_log (
    id serial PRIMARY KEY,
    audit_log_id integer REFERENCES pg_llm_audit_log(id),
    encrypted_details bytea NOT NULL,
    access_level text NOT NULL
);
```

### 4.2 Log Access
```c
typedef struct AuditAccess {
    char *user_id;
    char *action;
    char *object_type;
    char *object_id;
    Jsonb *details;
    Jsonb *client_info;
} AuditAccess;

// Log operations
int log_audit_event(AuditAccess *access);
AuditAccess **get_audit_logs(const char *user_id, int limit);
bool delete_old_logs(int days_to_keep);
```

### 4.3 Log Analysis
```c
typedef struct AuditAnalysis {
    char *user_id;
    int event_count;
    char **frequent_actions;
    time_t time_range[2];
    Jsonb *summary;
} AuditAnalysis;

// Analysis operations
AuditAnalysis *analyze_audit_logs(const char *user_id);
Jsonb *generate_audit_report(AuditAnalysis *analysis);
```

## 5. Data Encryption

### 5.1 Encryption Interface
```c
typedef struct Encryption {
    char *algorithm;
    int key_size;
    char *key_id;
    bool use_compression;
} Encryption;

// Encryption operations
bytea *encrypt_data(const char *data, Encryption *enc);
char *decrypt_data(bytea *encrypted_data, Encryption *enc);
```

### 5.2 Key Management
```c
typedef struct KeyManagement {
    char *key_id;
    bytea *key_material;
    time_t created_at;
    time_t expires_at;
    bool active;
} KeyManagement;

// Key operations
char *generate_key(int key_size);
bool rotate_key(const char *key_id);
bool revoke_key(const char *key_id);
```

### 5.3 Security Policy
```c
typedef struct SecurityPolicy {
    char **sensitive_fields;
    char **encryption_algorithms;
    int min_key_size;
    int key_rotation_days;
    bool enforce_encryption;
} SecurityPolicy;

// Policy operations
bool apply_security_policy(SecurityPolicy *policy);
bool validate_security_compliance(void);
```

## 6. Performance Optimization

### 6.1 Index Optimization
```sql
-- Configuration index
CREATE INDEX idx_config_key ON pg_llm_config(key);
CREATE INDEX idx_config_updated ON pg_llm_config(updated_at);

-- Session index
CREATE INDEX idx_session_user ON pg_llm_sessions(user_id);
CREATE INDEX idx_session_active ON pg_llm_sessions(last_active);

-- Audit index
CREATE INDEX idx_audit_user ON pg_llm_audit_log(user_id);
CREATE INDEX idx_audit_action ON pg_llm_audit_log(action);
CREATE INDEX idx_audit_time ON pg_llm_audit_log(timestamp);
```

### 6.2 Query Optimization
```c
typedef struct QueryOptimization {
    char *table_name;
    char **indexed_columns;
    char *partition_key;
    int partition_interval;
} QueryOptimization;

// Optimization operations
bool optimize_table(QueryOptimization *opt);
void analyze_query_performance(void);
```

### 6.3 Partition Strategy
```c
typedef struct PartitionStrategy {
    char *table_name;
    char *partition_column;
    char *partition_type;
    int retention_days;
} PartitionStrategy;

// Partition operations
bool create_partitions(PartitionStrategy *strategy);
bool manage_partitions(void);
``` 