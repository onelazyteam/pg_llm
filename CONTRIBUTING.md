# Development Guide

This document provides comprehensive instructions for developing and contributing to the pg_llm PostgreSQL extension.

## Code Organization

```
pg_llm/
├── include/                 # Public header files
│   ├── models/            # Model interface headers
│   └── utils/             # Utility header files
│       └── pg_llm_glog.h  # glog integration header
├── src/                    # Source files
│   ├── models/           # Model implementations
│   │   ├── chatgpt/     # ChatGPT model
│   │   ├── deepseek/    # DeepSeek model
│   │   ├── hunyuan/     # Tencent Hunyuan model
│   │   └── qianwen/     # Alibaba Qianwen model
│   └── utils/           # Utility implementations
│       └── pg_llm_glog.cpp  # glog integration implementation
├── test/                  # Test files
│   ├── sql/             # SQL test files
│   └── expected/        # Expected test outputs
├── thirdparty/           # Third-party dependencies
│   ├── build_glog.sh    # Script to build glog
│   ├── glog/            # glog source code (after build)
│   └── install/         # Installed libraries and headers
├── docs/                 # Documentation
├── .githooks/           # Git hooks for development
├── CMakeLists.txt       # Main CMake build configuration
├── src/CMakeLists.txt   # Source directory build configuration
├── pg_llm.control      # PostgreSQL extension control file
└── README.md           # Project overview
```

## Dependencies

Building pg_llm requires the following dependencies:

- CMake 3.15 or higher
- PostgreSQL 14.0 or higher
- C++20 compatible compiler (GCC 10+ or Clang 10+)
- libcurl
- OpenSSL
- jsoncpp
- pkg-config

### Installing Dependencies

#### MacOS

```bash
brew install postgresql curl jsoncpp openssl cmake pkg-config
# Ensure PostgreSQL is properly linked
brew link --force postgresql
```

#### Ubuntu/Debian

```bash
sudo apt-get install postgresql-server-dev-all libcurl4-openssl-dev libjsoncpp-dev libssl-dev cmake pkg-config
```

#### CentOS/RHEL

```bash
sudo yum install postgresql-devel libcurl-devel jsoncpp-devel openssl-devel cmake pkgconfig
```

## Building for Development

### Building glog Dependency

The pg_llm extension uses Google's logging library (glog) for structured logging. Before building pg_llm, the glog library needs to be compiled.

#### Automatic glog Build (Default)

By default, glog is automatically downloaded and built when you compile pg_llm with CMake. The build system will:
1. Run the `thirdparty/build_glog.sh` script
2. Download glog v0.7.0 from GitHub
3. Configure and build glog with appropriate options
4. Install the glog library to `thirdparty/install/`

#### Manual glog Build (Optional)

If you need to build glog manually (for debugging or customization):

```bash
cd thirdparty
./build_glog.sh
```

This will create the following directories:
- `thirdparty/glog/`: Contains the glog source code
- `thirdparty/glog/build/`: Contains the build files
- `thirdparty/install/`: Contains the installed headers and libraries

#### Verifying glog Build

To verify that glog was built correctly:

```bash
# Check for the indicator file
ls -la thirdparty/install/.glog_build_complete

# Check for the library file
ls -la thirdparty/install/lib/libglog.a

# Check for header files
ls -la thirdparty/install/include/glog/
```

### Building pg_llm

After the glog dependency is prepared, you can build pg_llm as follows:

#### Create Build Directory

```bash
cd pg_llm
mkdir build
cd build
```

#### Configure Project

CMake supports multiple build types:

#### Debug Version

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

Debug version includes debug symbols, disables optimization, suitable for development and debugging.

#### Release Version

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

Release version enables optimizations, suitable for production environments.

#### Memory Checking Version (ASan)

```bash
cmake -DCMAKE_BUILD_TYPE=ASan ..
```

Uses AddressSanitizer for memory issue detection, suitable for finding memory leaks and out-of-bounds access problems.

#### Compile

```bash
make
```

To accelerate compilation with multi-threading, use:

```bash
make -j$(nproc)  # Linux
# or
make -j$(sysctl -n hw.ncpu)  # MacOS
```

The build system will:
1. Download and build glog if needed (if not already built manually)
2. Include glog header directories in compilation flags
3. Link against the glog library
4. Set up everything required for pg_llm to use glog

#### Run Tests

```bash
make test
```

#### Install

```bash
sudo make install
```

#### Enabling the Extension in PostgreSQL

After installation, connect to your PostgreSQL database and execute:

```sql
CREATE EXTENSION pg_llm;
```

This will initialize glog as part of the extension loading process.

## Development Environment Setup

### CMake Integration with IDEs

This project supports CMake, which works well with modern IDEs like:

- **Visual Studio Code**: Use the CMake Tools extension
- **CLion**: Native CMake support
- **Visual Studio**: Use the CMake project support

Most IDEs with CMake support will automatically detect build targets, include paths, and provide better code navigation and completion.

### Generate Compilation Database

To support certain IDEs and code analysis tools, you can generate a compilation command database:

```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
```

This will generate a `compile_commands.json` file in the build directory.

## Development Workflow

1. Create a new feature or fix branch
2. Maintain proper commit message format during development
3. Submit a Pull Request when complete
4. Wait for code review and merge

## Adding a New Model

1. Create new model files:
   ```bash
   touch include/pg_llm/models/new_model.h
   touch src/models/new_model/new_model.cpp
   ```

2. Implement the LLMInterface:
   ```cpp
   class NewModel : public LLMInterface {
       // Implement required methods
   };
   ```

3. Register the model in `src/api/pg_llm.cpp`:
   ```cpp
   manager.register_model("new_model", []() {
       return std::make_unique<NewModel>();
   });
   ```

4. Add tests in `test/sql/test_pg_llm.sql`

## Coding Standards

- Use C++20 features appropriately
- Follow PostgreSQL coding conventions
- Add comprehensive comments
- Include unit tests for new features
- Update documentation as needed

## Commit Convention

To maintain a clean codebase and readable commit history, we enforce a strict commit message format.

### Commit Message Format

Each commit message must follow this format:
```
[type] #number message
```

Where:
- `type` must be one of:
  - `feature`: New feature development
  - `bug`: Bug fixes
  - `doc`: Documentation updates
  - `task`: General tasks and improvements
- `number` must be a number (corresponding to an issue or task number)
- `message` is a description of the changes made in this commit

Examples:
- `[feature] #123 Add vector similarity calculation`
- `[bug] #456 Fix memory leak issue`
- `[doc] #789 Update API documentation`
- `[task] #321 Optimize model loading performance`

### Git Hooks Setup

We use git hooks to automatically verify commit message format. In the pg_llm directory, run the following command to enable hooks:

```bash
git config core.hooksPath .githooks
chmod +x .githooks/commit-msg
```

### 🧹 Code Style
This project uses **clang-format** to enforce C++ code style (based on Google style).

- Run `./setup-clang-format.sh` after cloning.
- All commits will be checked with clang-format locally and on GitHub Actions.
- Fix formatting issues with:
  ```bash
  clang-format -i your_file.cpp

## Common Build Issues

### PostgreSQL Not Found

If CMake cannot find PostgreSQL, you can manually specify the pg_config path:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DPG_CONFIG=/path/to/pg_config ..
```

### Dependency Libraries Not Found

If CMake cannot find certain dependency libraries, you can manually specify their paths:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DOPENSSL_ROOT_DIR=/path/to/openssl \
      -DCURL_ROOT_DIR=/path/to/curl \
      ..
```

## Environment Variables for macOS

If you are developing on macOS, you may need to set these environment variables before running CMake:

```bash
# Get dependency paths
OPENSSL_PATH=$(brew --prefix openssl)
CURL_PATH=$(brew --prefix curl)
JSONCPP_PATH=$(brew --prefix jsoncpp)
POSTGRES_PATH=$(brew --prefix postgresql)

# Set environment variables
export PKG_CONFIG_PATH="${OPENSSL_PATH}/lib/pkgconfig:${CURL_PATH}/lib/pkgconfig:${JSONCPP_PATH}/lib/pkgconfig:${POSTGRES_PATH}/lib/pkgconfig:$PKG_CONFIG_PATH"
export LIBRARY_PATH="${OPENSSL_PATH}/lib:${CURL_PATH}/lib:${JSONCPP_PATH}/lib:${POSTGRES_PATH}/lib:$LIBRARY_PATH"
export DYLD_LIBRARY_PATH="${OPENSSL_PATH}/lib:${CURL_PATH}/lib:${JSONCPP_PATH}/lib:${POSTGRES_PATH}/lib:$DYLD_LIBRARY_PATH"
```

Then configure with dependency paths:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DOPENSSL_ROOT_DIR="${OPENSSL_PATH}" \
      -DCURL_ROOT_DIR="${CURL_PATH}" \
      -DJSONCPP_ROOT_DIR="${JSONCPP_PATH}" \
      ..
```

## Important Notes

- Each commit should represent a single, complete feature or fix
- Commit messages should clearly describe the changes and their purpose
- Follow the coding standards and development workflow
- Keep documentation up to date

## Logging

pg_llm uses Google Logging (glog) for logging. Please follow these best practices:

1. **Correctly include header files**: Always use our wrapper header file, do not include glog headers directly:
   ```cpp
   #include "utils/pg_llm_glog.h"  // Correct
   // #include <glog/logging.h>  // Wrong!
   ```

2. **Use safe logging macros**: Use our wrapper macros for logging:
   ```cpp
   PG_LLM_LOG_INFO("This is an info log");
   PG_LLM_LOG_WARNING("This is a warning log");
   PG_LLM_LOG_ERROR("This is an error log");
   PG_LLM_LOG_FATAL("This is a fatal log");
   ```

3. **Do not use original LOG macros**: Do not use glog's LOG macros directly, as they will conflict with PostgreSQL macros:
   ```cpp
   // LOG(INFO) << "this is wrong";  // Wrong!
   ```

For detailed logging system usage instructions, please refer to the [glog integration documentation](./docs/glog_integration.md). 