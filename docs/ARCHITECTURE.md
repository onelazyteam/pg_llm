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
             ┌───────▼──────┐    ┌───────▼──────┐    ┌───────▼──────┐
             │     Model    │    │   Session    │    │     Chat     │
             │   Manager    │    │   Manager    │    │  Interface   │
             └───────┬──────┘    └──────────────┘    └──────────────┘
                     │
        ┌───────────┼───────────┐
        │           │           │
┌───────▼───┐ ┌────▼────┐ ┌────▼────┐
│  ChatGPT  │ │ DeepSeek│ │ Hunyuan │
└───────────┘ └─────────┘ └─────────┘
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
- `LLMInterface`: Abstract interface for all LLM implementations
- Model Implementations: ChatGPT, DeepSeek, Hunyuan, Qianwen

### 2. Session Management Layer

Handles stateful chat interactions through:
- Session creation and tracking
- Message history management
- Context management
- Session cleanup

Database Schema:
```sql
CREATE TABLE pg_llm_sessions (
    session_id TEXT PRIMARY KEY,
    model_instance TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_used_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE pg_llm_session_history (
    id SERIAL PRIMARY KEY,
    session_id TEXT REFERENCES pg_llm_sessions(session_id),
    role TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### 3. SQL Interface Layer

Provides PostgreSQL functions for:

1. Model Management:
   - `pg_llm_add_model`: Register new model instances
   - `pg_llm_remove_model`: Remove model instances

2. Chat Operations:
   - `pg_llm_chat`: Single-turn chat
   - `pg_llm_parallel_chat`: Parallel chat with multiple models

3. Session Management:
   - `pg_llm_create_session`: Create chat sessions
   - `pg_llm_chat_session`: Chat within sessions

## Key Features

### 1. Model Support
- Multiple LLM provider support
- Extensible model interface
- Parallel inference capabilities
- Model configuration management

### 2. Session Management
- Stateful conversations
- Message history tracking
- Context management
- Session isolation

### 3. Security
- Secure API key storage
- Input validation and sanitization
- Session isolation
- SQL injection prevention

### 4. Error Handling
- Comprehensive error reporting
- Transaction safety
- API error handling
- Session state management

## Data Flow

1. Single-turn Chat:
```
User Query → SQL Function → Model Manager → LLM API → Response → User
```

2. Session Chat:
```
User Query → SQL Function → Session Manager → Message History → 
Model Manager → LLM API → Response → Session History → User
```

3. Parallel Chat:
```
User Query → SQL Function → Model Manager → Multiple LLM APIs → 
Response Aggregation → User
```

## Configuration

### Model Configuration
Example JSON configuration:
```json
{
    "model": "gpt-4",
    "temperature": 0.7,
    "max_tokens": 2000,
    "timeout": 30
}
```

### Session Configuration
- Session timeout settings
- History retention policy
- Context window size
- Rate limiting

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