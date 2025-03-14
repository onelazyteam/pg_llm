# pg_llm Directory Structure

This document provides an overview of the pg_llm directory structure and file organization.

## Top-level Organization

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

## Key Files and Their Purposes

### Main Extension Files

- `src/pg_llm.cpp`: Main extension entry point with PostgreSQL extension functions
- `pg_llm.control`: PostgreSQL extension control file

### glog Integration

- `include/utils/pg_llm_glog.h`: Header defining glog-related functions and GUC parameters
- `src/utils/pg_llm_glog.cpp`: Implementation of glog initialization and GUC parameters
- `thirdparty/build_glog.sh`: Script to download and build glog

### Model Implementation

- `include/models/*.h`: Header files for different model interfaces
- `src/models/*/`: Implementation files for specific LLM models

### Build System

- `CMakeLists.txt`: Top-level CMake configuration
- `src/CMakeLists.txt`: Source directory CMake configuration

## File Naming Conventions

- Header files have `.h` extension
- Implementation files have `.cpp` extension
- Directory names should be lowercase with underscores for spaces
- Header files should match their corresponding implementation files

## Adding New Files

When adding new files to the project:

1. Place header files in the appropriate subdirectory under `include/`
2. Place implementation files in the corresponding subdirectory under `src/`
3. Update the relevant `CMakeLists.txt` file if needed
4. Follow the established naming conventions and code style

## Documentation Files

Documentation files should be placed in the `docs/` directory and written in Markdown format. Each major feature or component should have its own documentation file. 