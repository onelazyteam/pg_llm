/*
 * pg_llm_glog.cpp
 *
 * Implementation of glog integration with pg_llm
 */

/* Include our header first */

/* Define macros for glog before including it */
#define GLOG_NO_ABBREVIATED_SEVERITIES
#define GOOGLE_GLOG_DLL_DECL
#define GLOG_USE_GLOG_EXPORT

#pragma push_macro("LOG")
#undef LOG
#include <glog/logging.h>
#pragma pop_macro("LOG")

#include "utils/pg_llm_glog.h"

extern "C" {
#include "postgres.h"
#include "fmgr.h"
#include "utils/guc.h"
#include "utils/builtins.h"
#include "miscadmin.h"
#include <string.h>
}

#include <sys/stat.h>
#include <errno.h>

/* GUC variables */
char *pg_llm_glog_log_dir = NULL;
char *pg_llm_glog_min_log_level = NULL;
bool pg_llm_glog_log_to_stderr = true;
bool pg_llm_glog_log_to_system_logger = false;
int pg_llm_glog_max_log_size = 50;  /* MB */
int pg_llm_glog_v = 0;  /* VLOG level */

/* Function to initialize GUC parameters */
void
pg_llm_glog_init_guc(void)
{
    /* Define custom GUC variables */
    DefineCustomStringVariable(
        "pg_llm.glog_log_dir",
        "Directory where glog will write log files",
        NULL,
        &pg_llm_glog_log_dir,
        DataDir,  /* Default to PostgreSQL data directory */
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL
    );

    DefineCustomStringVariable(
        "pg_llm.glog_min_log_level",
        "Minimum log level for glog (INFO, WARNING, ERROR, FATAL)",
        NULL,
        &pg_llm_glog_min_log_level,
        "INFO",
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL
    );

    DefineCustomBoolVariable(
        "pg_llm.glog_log_to_stderr",
        "Whether glog should log to stderr",
        NULL,
        &pg_llm_glog_log_to_stderr,
        true,
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL
    );

    DefineCustomBoolVariable(
        "pg_llm.glog_log_to_system_logger",
        "Whether glog should log to system logger",
        NULL,
        &pg_llm_glog_log_to_system_logger,
        false,
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL
    );

    DefineCustomIntVariable(
        "pg_llm.glog_max_log_size",
        "Maximum log file size in MB",
        NULL,
        &pg_llm_glog_max_log_size,
        50,  /* Default 50MB */
        1,   /* Min 1MB */
        1000, /* Max 1000MB */
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL
    );

    DefineCustomIntVariable(
        "pg_llm.glog_v",
        "VLOG verbosity level",
        NULL,
        &pg_llm_glog_v,
        0,    /* Default 0 */
        0,    /* Min 0 */
        9,    /* Max 9 */
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL
    );
}

/* Function to initialize glog */
void
pg_llm_glog_init(void)
{
    /* Initialize glog with program name */
    google::InitGoogleLogging("pg_llm");

    /* Set configuration based on GUC parameters */
    
    /* Set log directory */
    if (pg_llm_glog_log_dir) {
        char glog_path[MAXPGPATH];
        snprintf(glog_path, MAXPGPATH, "%s/glog", pg_llm_glog_log_dir);

        if (mkdir(glog_path, 0700) != 0 && errno != EEXIST) {
            ereport(ERROR,
                    (errcode_for_file_access(),
                     errmsg("could not create directory \"%s\": %m", glog_path)));
        }
        
        FLAGS_log_dir = glog_path;
    }
    
    /* Set min log level */
    if (pg_llm_glog_min_log_level) {
        if (strcmp(pg_llm_glog_min_log_level, "INFO") == 0)
            FLAGS_minloglevel = google::GLOG_INFO;
        else if (strcmp(pg_llm_glog_min_log_level, "WARNING") == 0)
            FLAGS_minloglevel = google::GLOG_WARNING;
        else if (strcmp(pg_llm_glog_min_log_level, "ERROR") == 0)
            FLAGS_minloglevel = google::GLOG_ERROR;
        else if (strcmp(pg_llm_glog_min_log_level, "FATAL") == 0)
            FLAGS_minloglevel = google::GLOG_FATAL;
    }
    
    /* Set log to stderr */
    FLAGS_logtostderr = pg_llm_glog_log_to_stderr;
    
    /* Set log to system logger */
    FLAGS_alsologtostderr = pg_llm_glog_log_to_system_logger;
    
    /* Set max log size */
    FLAGS_max_log_size = pg_llm_glog_max_log_size;
    
    /* Set VLOG level */
    FLAGS_v = pg_llm_glog_v;
    
    /* Log initialization message */
    pg_llm_log_info(__FILE__, __LINE__, "pg_llm glog initialized");
}

/* Function to shutdown glog */
void
pg_llm_glog_shutdown(void)
{
    /* Log shutdown message */
    pg_llm_log_info(__FILE__, __LINE__, "pg_llm glog shutting down");
    
    /* Shutdown glog */
    google::ShutdownGoogleLogging();
}

/* Implementation of wrapper functions */
void pg_llm_log_info(const char* file, int line, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    google::LogMessage(file, line, google::GLOG_INFO).stream() << buffer;
}

void pg_llm_log_warning(const char* file, int line, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    google::LogMessage(file, line, google::GLOG_WARNING).stream() << buffer;
}

void pg_llm_log_error(const char* file, int line, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    google::LogMessage(file, line, google::GLOG_ERROR).stream() << buffer;
}

void pg_llm_log_fatal(const char* file, int line, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    google::LogMessage(file, line, google::GLOG_FATAL).stream() << buffer;
} 