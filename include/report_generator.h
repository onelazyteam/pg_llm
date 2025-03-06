#ifndef PG_LLM_REPORT_GENERATOR_H
#define PG_LLM_REPORT_GENERATOR_H

#include "postgres.h"

/* Report type enumeration */
typedef enum ReportType {
    REPORT_TYPE_TEXT,
    REPORT_TYPE_CHART,
    REPORT_TYPE_TABLE,
    REPORT_TYPE_MIXED,
    REPORT_TREND,
    REPORT_COMPARISON,
    REPORT_SUMMARY,
    REPORT_FORECAST,
    REPORT_CUSTOM
} ReportType;

/* Report configuration structure */
typedef struct ReportConfig {
    ReportType type;
    char *title;
    char *description;
    bool include_chart;
    char *chart_type;
    bool include_table;
    bool include_summary;
    char *format;
} ReportConfig;

/* Function declarations */
char* generate_report(const char *description);
char* generate_typed_report(const char *description, ReportType type);
char* execute_report_sql(const char *sql);
Datum pg_llm_generate_report(PG_FUNCTION_ARGS);
Datum pg_llm_get_visualization_url(PG_FUNCTION_ARGS);

#endif /* PG_LLM_REPORT_GENERATOR_H */ 