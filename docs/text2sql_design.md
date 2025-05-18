# Text2SQL Design Document

## 1. Overview

Text2SQL is a PostgreSQL extension that converts natural language queries into SQL statements. It leverages Large Language Models (LLMs) and vector search technology to help users query databases using natural language.

## 2. Architecture

### 2.1 Core Components

1. **Text2SQL Class**
   - Core conversion engine
   - Manages LLM model and configuration
   - Handles schema information and vector search

2. **Data Structures**
   - `TableInfo`: Table structure information
   - `VectorSchemaInfo`: Vector search schema results
   - `Text2SQLConfig`: Configuration parameters
   - `CacheEntry`: Cache entry with timestamp

### 2.2 Workflow

1. **Schema Information Retrieval**
   - Fetches table structure from database
   - Supports custom schema information
   - Implements caching mechanism

2. **Vector Search**
   - Uses pgvector for similarity search
   - Configurable similarity threshold
   - Supports parallel processing
   - Implements batch processing

3. **SQL Generation**
   - Builds optimized prompts
   - Calls LLM for SQL generation
   - Extracts and optimizes SQL statements
   - Implements result caching

## 3. Configuration

### 3.1 Basic Settings

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| use_vector_search | bool | true | Enable vector search |
| max_tables | int | 5 | Maximum number of tables to return |
| similarity_threshold | float | 0.7 | Similarity threshold for vector search |
| max_tokens | int | 4000 | Maximum token limit |
| include_sample_data | bool | true | Include sample data in prompts |
| sample_data_limit | int | 3 | Sample data row limit |

### 3.2 Performance Settings

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| enable_cache | bool | true | Enable caching |
| cache_ttl_seconds | int | 3600 | Cache time-to-live in seconds |
| max_cache_size | int | 1000 | Maximum cache entries |
| batch_size | int | 100 | Batch size for vector operations |
| parallel_processing | bool | true | Enable parallel processing |
| max_parallel_threads | int | 4 | Maximum parallel threads |

## 4. Performance Optimization

### 4.1 Caching Mechanism

1. **Multi-level Cache**
   - Schema information cache
   - Sample data cache
   - Vector search results cache
   - SQL generation cache

2. **Cache Management**
   - Time-based expiration
   - Size-based cleanup
   - Thread-safe operations

3. **Cache Configuration**
   - Configurable TTL
   - Adjustable cache size
   - Enable/disable per cache type

### 4.2 Parallel Processing

1. **Vector Search**
   - Parallel embedding generation
   - Batch processing of vectors
   - Configurable thread count

2. **Performance Tuning**
   - Adjustable batch size
   - Dynamic thread management
   - Resource monitoring

### 4.3 Memory Management

1. **Resource Optimization**
   - Efficient memory usage
   - Timely cache cleanup
   - Memory leak prevention

2. **Query Optimization**
   - Reduced database queries
   - Batch processing
   - Query result caching

## 5. Usage Examples

### 5.1 Basic Usage

```sql
-- Basic text2sql query
SELECT pg_llm_text2sql('model_name', 'query all user information');

-- With custom schema
SELECT pg_llm_text2sql('model_name', 'query all user information', '{"tables":[...]}');

-- Disable vector search
SELECT pg_llm_text2sql('model_name', 'query all user information', NULL, false);
```

### 5.2 Performance Tuning

```sql
-- Configure performance settings
SELECT pg_llm_text2sql('model_name', 'query all user information', NULL, true,
  '{"enable_cache": true, "cache_ttl_seconds": 7200, "parallel_processing": true}');
```

## 6. Monitoring and Tuning

### 6.1 Performance Metrics

1. **Cache Metrics**
   - Cache hit rate
   - Cache size
   - Cache eviction rate

2. **Processing Metrics**
   - Query latency
   - Vector search time
   - SQL generation time

3. **Resource Usage**
   - Memory consumption
   - CPU utilization
   - Database load

### 6.2 Tuning Guidelines

1. **Cache Configuration**
   - Monitor cache hit rates
   - Adjust cache size based on usage
   - Set appropriate TTL values

2. **Parallel Processing**
   - Monitor CPU utilization
   - Adjust thread count
   - Optimize batch size

3. **Resource Management**
   - Monitor memory usage
   - Optimize query patterns
   - Balance load distribution

## 7. Security Considerations

1. **SQL Injection Prevention**
   - Strict SQL extraction
   - Parameter validation
   - Query sanitization

2. **Access Control**
   - PostgreSQL permissions
   - Data access restrictions
   - API key management

3. **Data Protection**
   - Sensitive data handling
   - Cache security
   - Audit logging

## 8. Future Improvements

1. **Feature Enhancements**
   - Support for more SQL dialects
   - Query optimization suggestions
   - Advanced caching strategies

2. **Performance Optimization**
   - Improved vector search
   - Enhanced parallel processing
   - Better resource management

3. **User Experience**
   - More configuration options
   - Detailed error reporting
   - Performance monitoring tools

## 9. Dependencies

1. **Required Extensions**
   - pgvector
   - LLM interface
   - PostgreSQL 12+

2. **System Requirements**
   - Adequate memory
   - Multi-core CPU
   - Sufficient disk space

## 10. References

1. PostgreSQL Documentation
2. pgvector Documentation
3. LLM Model Documentation 