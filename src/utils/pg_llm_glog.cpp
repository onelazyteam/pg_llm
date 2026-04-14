#include "utils/pg_llm_glog.h"

extern "C" {
#include "fmgr.h"
#include "utils/guc.h"
}

#include <cstdarg>
#include <cstdio>

char* pg_llm_glog_log_dir = nullptr;
char* pg_llm_glog_min_log_level = nullptr;
bool pg_llm_glog_log_to_stderr = true;
bool pg_llm_glog_log_to_system_logger = false;
int pg_llm_glog_max_log_size = 50;
int pg_llm_glog_v = 0;

extern "C" void pg_llm_glog_init_guc(void) {
  DefineCustomStringVariable("pg_llm.glog_log_dir",
                             "Compatibility GUC retained for older setups.",
                             nullptr,
                             &pg_llm_glog_log_dir,
                             nullptr,
                             PGC_SUSET,
                             0,
                             nullptr,
                             nullptr,
                             nullptr);

  DefineCustomStringVariable("pg_llm.glog_min_log_level",
                             "Compatibility GUC retained for older setups.",
                             nullptr,
                             &pg_llm_glog_min_log_level,
                             "INFO",
                             PGC_SUSET,
                             0,
                             nullptr,
                             nullptr,
                             nullptr);

  DefineCustomBoolVariable("pg_llm.glog_log_to_stderr",
                           "Compatibility GUC retained for older setups.",
                           nullptr,
                           &pg_llm_glog_log_to_stderr,
                           true,
                           PGC_SUSET,
                           0,
                           nullptr,
                           nullptr,
                           nullptr);

  DefineCustomBoolVariable("pg_llm.glog_log_to_system_logger",
                           "Compatibility GUC retained for older setups.",
                           nullptr,
                           &pg_llm_glog_log_to_system_logger,
                           false,
                           PGC_SUSET,
                           0,
                           nullptr,
                           nullptr,
                           nullptr);

  DefineCustomIntVariable("pg_llm.glog_max_log_size",
                          "Compatibility GUC retained for older setups.",
                          nullptr,
                          &pg_llm_glog_max_log_size,
                          50,
                          1,
                          1024,
                          PGC_SUSET,
                          0,
                          nullptr,
                          nullptr,
                          nullptr);

  DefineCustomIntVariable("pg_llm.glog_v",
                          "Compatibility GUC retained for older setups.",
                          nullptr,
                          &pg_llm_glog_v,
                          0,
                          0,
                          32,
                          PGC_SUSET,
                          0,
                          nullptr,
                          nullptr,
                          nullptr);
}

extern "C" void pg_llm_glog_init(void) {}

extern "C" void pg_llm_glog_shutdown(void) {}

static void pg_llm_vlog(int elevel, const char* file, int line, const char* format, va_list args) {
  char buffer[2048];
  vsnprintf(buffer, sizeof(buffer), format, args);
  ereport(elevel, (errmsg("pg_llm [%s:%d] %s", file, line, buffer)));
}

void pg_llm_log_info(const char* file, int line, const char* format, ...) {
  va_list args;
  va_start(args, format);
  pg_llm_vlog(LOG, file, line, format, args);
  va_end(args);
}

void pg_llm_log_warning(const char* file, int line, const char* format, ...) {
  va_list args;
  va_start(args, format);
  pg_llm_vlog(WARNING, file, line, format, args);
  va_end(args);
}

void pg_llm_log_error(const char* file, int line, const char* format, ...) {
  va_list args;
  va_start(args, format);
  pg_llm_vlog(WARNING, file, line, format, args);
  va_end(args);
}

void pg_llm_log_fatal(const char* file, int line, const char* format, ...) {
  va_list args;
  va_start(args, format);
  pg_llm_vlog(ERROR, file, line, format, args);
  va_end(args);
}
