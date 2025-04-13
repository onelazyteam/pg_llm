# pg_llm Architecture Design

## Overview

pg_llm is a PostgreSQL extension that integrates Large Language Models (LLMs) into PostgreSQL, enabling direct interaction with various LLM services through SQL interfaces. The extension is designed to be modular, extensible, and secure, supporting both stateless and stateful chat interactions.

## System Architecture

```
                                  ┌─────────────────┐
                                  │   PostgreSQL    │
                                  └────────┬────────┘
                                          │
                                  ┌───────▼────────┐
                                  │    pg_llm      │
                                  └───────┬────────┘
                     ┌────────────────────┼────────────────────┐
                     │                    │                    │
             ┌───────▼──────┐     ┌───────▼──────┐     ┌───────▼──────┐
             │     Model    │     │    NL2SQL    │     │     Chat     │
             │   Manager    │     │   Text2SQL   │     │   Interface  │
             └───────┬──────┘     └──────────────┘     └──────────────┘
                     │
         ┌───────────┼───────────┐
         │           │           │
 ┌───────▼───┐  ┌────▼────┐ ┌────▼────┐
 │  ChatGPT  │  │ DeepSeek│ │   ...   │
 └───────────┘  └─────────┘ └─────────┘
```

## Core Components

### 1. Model Management Layer

The Model Management Layer is responsible for:
- Model registration and instantiation
- API key and configuration management
- Model lifecycle management
- Parallel inference coordination

Key Components:
- `ModelManager`: Singleton class managing model instances
- `LLMInterface`: Interface for all LLM implementations, such as DeepSeek, Qianwen and so on.

### 2. SQL Interface Layer

Provides PostgreSQL functions for:

1. Model Management:
   - `pg_llm_add_model`: Register new model instances
   - `pg_llm_remove_model`: Remove model instances

2. Chat Operations:
   - `pg_llm_chat`: Single-turn chat
   - `pg_llm_parallel_chat`: Parallel chat with multiple models

3. Text2SQL:
   - `pg_llm_text2sql`: Convert natural language into SQL and execute the output results

## Key Features

### 1. Model Support
- Multiple LLM provider support
- Extensible model interface
- Parallel inference capabilities
- Model configuration management

### 2. Security
- Secure API key storage
- Input validation and sanitization
- SQL injection prevention

### 3. Error Handling
- Comprehensive error reporting
- Transaction safety
- API error handling

## Data Flow

1. Single-turn Chat:
```
User Query → SQL Function → Model Manager → LLM API → Response → User
```

2. Parallel Chat:
```
User Query → SQL Function → Model Manager → Multiple LLM APIs → 
Response Aggregation → User
```

## Future Enhancements

1. Advanced Features
   - Streaming responses
   - Batch processing
   - Model fine-tuning support
   - Vector similarity search

2. Performance Optimizations
   - Connection pooling
   - Response caching
   - Batch requests
   - Async processing

3. Monitoring and Management
   - Usage statistics
   - Performance metrics
   - Cost tracking
   - Health monitoring

4. Additional Capabilities
   - More model providers
   - Custom model hosting
   - Advanced prompt management
   - Result post-processing 