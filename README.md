# pg_llm - PostgreSQL LLM Integration Extension

This PostgreSQL extension enables direct integration with various Large Language Models (LLMs). It supports multiple LLM providers and features like session management, parallel inference, and more.

## Features

- [x] Support remote models
- [x] Support local LLM
- [x] Dynamic model management (add/remove models at runtime)
- [x] Large model metadata persistence
- [x] Importing the log library
- [ ] Support for streaming responses
- [x] Session-based multi-turn conversation support
- [x] Parallel inference with multiple models
- [ ] Automatically select the model with the highest confidence (select by score), and use the local model as a backup (fall back to the local model when confidence is low)
- [ ] Sensitive information encryption
- [ ] Audit logging
- [ ] Session state management
- [ ] Execute SQL through plug-in functions and let the LLM give optimization suggestions
- [x] Support natural language query, can enter human language through plug-ins to query database data
- [ ] Generate reports, generate data analysis charts, intelligent analysis: built-in data statistical analysis and visualization suggestion generation
- [ ] Full observability, complete record of decision-making process and intermediate results
- [ ] Improve model accuracy, RAG knowledge base, feedback learning

## Prerequisites

- PostgreSQL 14.0 or later
- C++20 compatible compiler
- libcurl
- OpenSSL
- jsoncpp
- CMake 3.15 or later (for CMake build)

## Dependencies

The pg_llm extension depends on the following libraries:

- **OpenSSL**: For secure connections to LLM APIs
- **cURL**: For making HTTP requests to LLM APIs
- **JsonCpp**: For parsing JSON responses from LLM APIs
- **Google Logging Library (glog)**: For structured logging (automatically downloaded and built)

The build system will automatically check for these dependencies and provide instructions if they are missing.

### Logging with glog

pg_llm uses Google's logging library (glog) for structured logging. The library is automatically downloaded and built during the compilation process. You can configure logging behavior using PostgreSQL configuration parameters:

See [thirdparty/README.md](thirdparty/README.md) for more details on glog integration and configuration.

## Installation

1. Configure PostgreSQL environment:

```bash
# Add PostgreSQL binaries to your PATH
# First, locate your PostgreSQL installation's bin directory:
# You can use the command:
which psql
# or
pg_config --bindir

# Then add the path to your shell configuration file
# For macOS (add to ~/.zshrc or ~/.bash_profile):
export PATH=/path/to/postgresql/bin:$PATH

# For Linux (add to ~/.bashrc):
export PATH=/path/to/postgresql/bin:$PATH

# Apply changes
source ~/.zshrc  # or ~/.bash_profile or ~/.bashrc
```

2. Install dependencies:

```bash
# Ubuntu/Debian
sudo apt-get install postgresql-server-dev-all libcurl4-openssl-dev libjsoncpp-dev libssl-dev cmake pkg-config

# MacOS
brew install postgresql curl jsoncpp openssl cmake pkg-config
```

3. Build and install glog:

```bash
cd pg_llm/thirdparty

# Build glog
./build_glog.sh

# Verify glog installation
ls -la install/lib/libglog.a        # Static library
ls -la install/include/glog/        # Header files
cat install/.glog_build_complete    # Build completion marker
```

The script will:
- Download glog v0.6.0 from GitHub
- Configure and build with appropriate options
- Install to `thirdparty/install/` directory
- Create static library and headers
- Set up proper linking flags

4. Build and install pg_llm:

```bash
cd pg_llm
mkdir build && cd build

# Configure (choose one of the following build types)
# Debug build with glog debug output enabled
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build with optimized glog settings
cmake -DCMAKE_BUILD_TYPE=Release ..

# Address Sanitizer build
cmake -DCMAKE_BUILD_TYPE=ASan ..

# Build
make

# Install
sudo make install
```

5. Configure PostgreSQL to load the extension:

Since pg_llm implements the `_PG_init` function for initialization, it must be loaded via `shared_preload_libraries`. Add the following to your `postgresql.conf` file:

```
# Add pg_llm to shared_preload_libraries
shared_preload_libraries = 'pg_llm'
```

6. Restart PostgreSQL to load the extension:

```bash
# For systemd-based systems
sudo systemctl restart postgresql

# For macOS
brew services restart postgresql

# For other systems
pg_ctl restart -D /path/to/data/directory
```

7. Create the extension in your database:
After building, you need to enable the extension in PostgreSQL:

```sql
-- Enable the extension
CREATE EXTENSION vector;
CREATE EXTENSION pg_llm;

-- Verify installation
SELECT * FROM pg_available_extensions WHERE name = 'pg_llm';
```

## Usage

### Adding Models

1. Alibaba Tongyi Qianwen:
```sql
SELECT pg_llm_add_model(
    'qianwen',
    'qianwen-chat',
    'your-api-key',
    '{
        "model_name": "qwen-turbo",
        "api_endpoint": "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation",
        "access_key_id": "your-access-key-id",
        "access_key_secret": "your-access-key-secret"
    }'
);
```

### Single-turn Chat

```sql
SELECT pg_llm_chat('gpt4-chat', 'What is PostgreSQL?');
```

### Multi-turn Chat

```sql
SELECT pg_llm_create_session();
SELECT pg_llm_multi_turn_chat('qianwen-chat', '5PN2qmWqBlQ9wQj99nsQzldVI5ZuGXbE', 'WHO ARE YOU?');
SELECT pg_llm_multi_turn_chat('qianwen-chat', '5PN2qmWqBlQ9wQj99nsQzldVI5ZuGXbE', 'What was the previous question?');
```

### Parallel Multi-model Chat

```sql
SELECT pg_llm_parallel_chat(
    'What are the advantages of PostgreSQL?',
    ARRAY['gpt4', 'deepseek-chat', 'hunyuan-chat', 'qianwen-chat']
);
```

### Removing Models

```sql
SELECT pg_llm_remove_model('gpt4-chat');
```

## Development Guide

Please refer to [CONTRIBUTING.md](CONTRIBUTING.md) for detailed development guidelines, including:
- Code organization
- Building for development
- Development workflow
- Adding new models
- Coding standards
- Commit conventions

## Security Considerations

- API keys are encrypted before storage
- All sensitive information is handled securely
- Access control is managed through PostgreSQL's permission system
- Comprehensive audit logging

## Contributing

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

### Coding Standards

- Use C++20 features appropriately
- Follow PostgreSQL coding conventions
- Add comprehensive comments
- Include unit tests for new features
- Update documentation as needed

## License

This project is licensed under the PostgreSQL License - see the LICENSE file for details.

## Support

For issues and feature requests, please create an issue in the GitHub repository. 