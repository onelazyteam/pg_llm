# Google Logging (glog) Integration

This document provides detailed instructions on how to correctly integrate and use Google's logging library (glog) in the pg_llm project.

## Introduction

Google Logging Library (glog) provides a high-performance, thread-safe and feature-rich logging system, which is very suitable for large C++ projects. We have integrated glog into pg_llm to achieve better logging and debugging capabilities.

## Header Files

To avoid conflicts with PostgreSQL macros, you must follow a specific order and macro definitions when including glog headers:

```cpp
// Always use our wrapper header
#include "utils/pg_llm_glog.h"  // Located in include/utils/pg_llm_glog.h

// Do not include glog/logging.h directly
// Wrong: #include <glog/logging.h>
```

## Logging Macros

We provide the following logging macros that support format strings:

```cpp
// Basic usage
PG_LLM_LOG_INFO("Simple message");

// With format strings
PG_LLM_LOG_INFO("Value: %d, String: %s", count, text.c_str());
PG_LLM_LOG_WARNING("Warning: %s (code: %d)", error_msg, error_code);
PG_LLM_LOG_ERROR("Error occurred: %s", error.what());
PG_LLM_LOG_FATAL("Fatal error in %s: %s", function_name, error_msg);

// Verbose logging (level can be configured via pg_llm.glog_v GUC variable)
PG_LLM_VLOG(1) << "Detailed debug info";
```

## Configuration

The glog library is automatically initialized when the module is loaded, and can be configured through the following GUC variables:

| Parameter | Description | Default | Range |
|-----------|-------------|---------|--------|
| `pg_llm.glog_log_dir` | Log file directory | PostgreSQL data directory | Any valid path |
| `pg_llm.glog_min_log_level` | Minimum log level | INFO | INFO, WARNING, ERROR, FATAL |
| `pg_llm.glog_log_to_stderr` | Output to stderr | true | true/false |
| `pg_llm.glog_log_to_system_logger` | Use system logger | false | true/false |
| `pg_llm.glog_max_log_size` | Maximum log file size (MB) | 50 | 1-1000 |
| `pg_llm.glog_v` | Verbose log level | 0 | 0-9 |

Example configuration:

```sql
-- Set glog configuration
ALTER SYSTEM SET pg_llm.glog_log_dir = '/path/to/logs';
ALTER SYSTEM SET pg_llm.glog_min_log_level = 'WARNING';
ALTER SYSTEM SET pg_llm.glog_max_log_size = 100;

-- Reload configuration
SELECT pg_reload_conf();
```

## Log File Format

glog creates log files with names in the following format:

```
pg_llm.[hostname].[user name].log.[severity level].[date].[time].[pid]
```

Example:
```
pg_llm.example.postgres.log.INFO.20240301-123456.12345
```

## CMake Configuration

In CMake, ensure glog is configured correctly:

```cmake
# Find glog dependency
find_package(glog REQUIRED)

# Add necessary macro definitions
add_compile_definitions(
    GLOG_NO_ABBREVIATED_SEVERITIES
    GOOGLE_GLOG_DLL_DECL
)

# Link glog library
target_link_libraries(your_target 
    glog::glog
    # Other libraries...
)
```

## Common Issues and Solutions

1. **"LOG" macro redefinition error**
   - Cause: Both PostgreSQL and glog define the LOG macro
   - Solution: Always use our wrapper header `pg_llm_glog.h`

2. **Format string errors**
   - Cause: Incorrect number or type of arguments
   - Solution: Ensure format specifiers match the provided arguments
   ```cpp
   // Correct:
   PG_LLM_LOG_INFO("Count: %d", count);
   // Wrong:
   PG_LLM_LOG_INFO("Count: %d"); // Missing argument
   ```

3. **Undefined symbols or linking errors**
   - Cause: Not correctly linking the glog library
   - Solution: Check CMakeLists.txt for proper library linking

4. **Logs not appearing**
   - Cause: Incorrect log directory permissions or configuration
   - Solution: 
     - Check GUC variable settings
     - Verify directory permissions
     - Check `pg_llm.glog_log_to_stderr` setting

## Best Practices

1. **Log Level Selection**
   - Use INFO for general operational information
   - Use WARNING for potential issues that don't affect operation
   - Use ERROR for issues that affect operation but don't require shutdown
   - Use FATAL for critical errors requiring immediate shutdown

2. **Format String Usage**
   ```cpp
   // Good:
   PG_LLM_LOG_INFO("Processing user %s: %d records", username, count);
   
   // Better (with context):
   PG_LLM_LOG_INFO("Function %s: Processing user %s: %d records", 
                   __func__, username, count);
   ```

3. **Performance Considerations**
   - Use VLOG for detailed debug information
   - Avoid expensive operations in log statements
   - Consider log file rotation settings

## Security Considerations

1. **Sensitive Data**
   - Never log sensitive information (passwords, API keys, etc.)
   - Be cautious with user input in log messages
   - Consider data privacy regulations

2. **File Permissions**
   - Log directory is created with 0700 permissions
   - Ensure proper ownership of log files
   - Monitor log file access

## Troubleshooting

1. **Viewing Logs**
   ```bash
   # View all logs
   tail -f /path/to/logs/pg_llm.*.log
   
   # View only error logs
   tail -f /path/to/logs/pg_llm.*.ERROR.*
   ```

2. **Common Debug Steps**
   - Check log directory exists and has correct permissions
   - Verify GUC settings are applied
   - Enable stderr logging for immediate feedback
   - Check PostgreSQL's error log for initialization issues

## Contributing

When adding new logging functionality:

1. Use the provided macros and headers
2. Follow the format string guidelines
3. Document any new logging patterns
4. Add appropriate error handling
5. Test with different log levels and configurations 