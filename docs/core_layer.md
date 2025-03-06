# pg_llm Core Layer Design

## 1. Query Parsing

### 1.1 Natural Language Parsing
```c
typedef struct NLQuery {
    char *original_text;
    char *normalized_text;
    char *intent;
    char **entities;
    int entity_count;
} NLQuery;

// Query parsing functions
NLQuery *parse_natural_language(const char *query);

// Intent detection
char *detect_intent(const char *query);

// Entity extraction
char **extract_entities(const char *query, int *count);
```

### 1.2 SQL Parsing
```c
typedef struct SQLQuery {
    char *original_sql;
    char *normalized_sql;
    char **tables;
    int table_count;
    char **columns;
    int column_count;
    char *where_clause;
    char *order_by;
    char *group_by;
} SQLQuery;

// SQL parsing functions
SQLQuery *parse_sql(const char *sql);

// Syntax check
bool validate_sql_syntax(const char *sql);

// Dependency analysis
char **analyze_dependencies(SQLQuery *query);
```

## 2. Model Invocation

### 2.1 Call Interface
```c
typedef struct ModelRequest {
    char *model_name;
    char *prompt;
    char *system_message;
    int max_tokens;
    float temperature;
    char **stop_words;
    int stop_word_count;
} ModelRequest;

typedef struct ModelResponse {
    char *text;
    float confidence;
    char *model_used;
    int tokens_used;
    bool successful;
    char *error;
} ModelResponse;

// Model invocation functions
ModelResponse *call_model(ModelRequest *request);
```

### 2.2 Prompt Engineering
```c
// Prompt template
typedef struct PromptTemplate {
    char *name;
    char *template;
    char **required_vars;
    int var_count;
} PromptTemplate;

// Prompt generation
char *generate_prompt(PromptTemplate *template, char **vars);

// Context management
char *build_context(char **history, int count);
```

### 2.3 Result Processing
```c
// Result parsing
typedef struct ParsedResult {
    char *sql;
    float confidence;
    char **suggestions;
    int suggestion_count;
} ParsedResult;

// Result validation
bool validate_result(ParsedResult *result);

// Result optimization
ParsedResult *optimize_result(ParsedResult *result);
```

## 3. Report Generation

### 3.1 Data Processing
```c
typedef struct ReportData {
    char **columns;
    int column_count;
    char ***rows;
    int row_count;
    char *summary;
} ReportData;

// Data aggregation
ReportData *aggregate_data(SQLQuery *query);

// Data transformation
char *transform_data(ReportData *data, const char *format);
```

### 3.2 Chart Generation
```c
typedef struct ChartConfig {
    char *type;
    char *title;
    char *x_axis;
    char *y_axis;
    bool show_legend;
    char *theme;
} ChartConfig;

// Chart generation
char *generate_chart(ReportData *data, ChartConfig *config);

// Layout optimization
void optimize_layout(ChartConfig *config);
```

### 3.3 Report Assembly
```c
typedef struct Report {
    char *id;
    char *title;
    char *description;
    char *chart_data;
    char *table_data;
    char *summary;
    time_t created_at;
} Report;

// Report assembly
Report *assemble_report(
    ReportData *data,
    ChartConfig *config,
    const char *title
);

// Report formatting
char *format_report(Report *report);
```

## 4. Visualization Generation

### 4.1 Visualization Service
```c
typedef struct VisualizationServer {
    char *host;
    int port;
    char *base_path;
    int timeout;
} VisualizationServer;

// Service initialization
void init_visualization_server(VisualizationServer *server);

// Health check
bool check_server_health(VisualizationServer *server);
```

### 4.2 Data Transfer
```c
typedef struct VisualizationData {
    char *report_id;
    char *data;
    char *config;
    time_t expires_at;
} VisualizationData;

// Data upload
bool upload_visualization_data(
    VisualizationServer *server,
    VisualizationData *data
);

// URL generation
char *generate_url(
    VisualizationServer *server,
    const char *report_id
);
```

### 4.3 Cache Management
```c
typedef struct VisualizationCache {
    char *report_id;
    char *url;
    time_t created_at;
    time_t expires_at;
} VisualizationCache;

// Cache operations
void cache_visualization(VisualizationCache *cache);
char *get_cached_url(const char *report_id);
```

## 5. Result Optimization

### 5.1 Query Optimization
```c
typedef struct QueryOptimization {
    char *original_query;
    char *optimized_query;
    char **suggestions;
    int suggestion_count;
} QueryOptimization;

// Query optimization
QueryOptimization *optimize_query(const char *query);

// Execution plan analysis
char *analyze_execution_plan(const char *query);
```

### 5.2 Result Optimization
```c
typedef struct ResultOptimization {
    char *original_result;
    char *optimized_result;
    float improvement;
    char **metrics;
    int metric_count;
} ResultOptimization;

// Result optimization
ResultOptimization *optimize_result(const char *result);

// Performance analysis
char *analyze_performance(ResultOptimization *opt);
```

## 6. Error Handling

### 6.1 Error Detection
```c
typedef struct ErrorDetection {
    char *query;
    char *error_type;
    char *message;
    char **suggestions;
    int suggestion_count;
} ErrorDetection;

// Error detection
ErrorDetection *detect_errors(const char *query);

// Error classification
char *classify_error(const char *error);
```

### 6.2 Error Recovery
```c
typedef struct ErrorRecovery {
    ErrorDetection *error;
    char *recovery_plan;
    bool can_recover;
    char *alternative;
} ErrorRecovery;

// Error recovery
ErrorRecovery *plan_recovery(ErrorDetection *error);

// Execute recovery
bool execute_recovery(ErrorRecovery *recovery);
```

## 7. Performance Monitoring

### 7.1 Performance Metrics
```c
typedef struct CoreMetrics {
    double query_parse_time;
    double model_call_time;
    double report_gen_time;
    double visualization_time;
    int memory_usage;
} CoreMetrics;

// Metrics collection
void collect_core_metrics(CoreMetrics *metrics);

// Performance report
char *generate_performance_report(CoreMetrics *metrics);
```

### 7.2 Performance Optimization
```c
typedef struct PerformanceOptimization {
    CoreMetrics *metrics;
    char **bottlenecks;
    int bottleneck_count;
    char **suggestions;
    int suggestion_count;
} PerformanceOptimization;

// Performance analysis
PerformanceOptimization *analyze_performance(CoreMetrics *metrics);

// Apply optimization
bool apply_optimization(PerformanceOptimization *opt);
``` 