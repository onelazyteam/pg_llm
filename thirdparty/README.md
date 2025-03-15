# Third-party Dependencies for pg_llm

This directory contains scripts and configurations for managing third-party dependencies used by the pg_llm PostgreSQL extension.

## Google Logging Library (glog)

The `build_glog.sh` script in this directory automates the process of downloading, building, and installing Google's logging library (glog) for use with pg_llm.

### Features

- Automatically downloads glog from GitHub (version v0.6.0)
- Configures and builds glog with appropriate options for PostgreSQL integration
- Installs glog to the local `install/` directory
- Provides structured logging across the extension
- Configurable via PostgreSQL GUC parameters

### How glog Integration Works

The integration with glog works as follows:

1. When pg_llm is built, the CMake build system automatically runs `build_glog.sh`
2. The script downloads glog source code from GitHub and builds it
3. The compiled libraries are stored in `thirdparty/install/`
4. When PostgreSQL loads the pg_llm extension, the `_PG_init()` function initializes glog
5. When PostgreSQL unloads the extension, the `_PG_fini()` function shuts down glog

### Building glog

The glog library is automatically built when you compile pg_llm with CMake. 
However, if you want to build it manually, you can run:

```bash
cd thirdparty
./build_glog.sh
```

### GUC Parameters

Once pg_llm is installed, you can configure glog using the following PostgreSQL parameters:

| Parameter | Description | Default | Type |
|-----------|-------------|---------|------|
| `pg_llm.glog_log_dir` | Directory where glog will write log files | PostgreSQL data directory | string |
| `pg_llm.glog_min_log_level` | Minimum log level (INFO, WARNING, ERROR, FATAL) | INFO | string |
| `pg_llm.glog_log_to_stderr` | Whether to log to stderr | true | boolean |
| `pg_llm.glog_log_to_system_logger` | Whether to log to system logger | false | boolean |
| `pg_llm.glog_max_log_size` | Maximum log file size in MB | 50 | integer |
| `pg_llm.glog_v` | VLOG verbosity level (0-9) | 0 | integer |

Example:

```sql
-- Set glog configuration
ALTER SYSTEM SET pg_llm.glog_log_dir = '/path/to/logs';
ALTER SYSTEM SET pg_llm.glog_min_log_level = 'WARNING';
ALTER SYSTEM SET pg_llm.glog_max_log_size = 100;

-- Reload configuration
SELECT pg_reload_conf();
```

### Log File Format

glog creates log files with names in the following format:

```
[program name].[hostname].[user name].log.[severity level].[date].[time].[pid]
```

For example:
```
pg_llm.example.postgres.log.INFO.20230301-123456.12345
```

### Logging in pg_llm Code

pg_llm developers can use glog in their code by including the glog header and using the logging macros:

```cpp
#include <glog/logging.h>

// Log at different levels
LOG(INFO) << "This is an info message";
LOG(WARNING) << "This is a warning message";
LOG(ERROR) << "This is an error message";

// Conditional logging
LOG_IF(INFO, condition) << "This is logged only if condition is true";

// Verbose logging (controlled by pg_llm.glog_v parameter)
VLOG(1) << "This is a verbose message at level 1";
```