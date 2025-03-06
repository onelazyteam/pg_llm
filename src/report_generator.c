#include "../include/report_generator.h"
#include "../include/model_interface.h"
#include "postgres.h"
#include "executor/spi.h"
#include <string.h>
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonb.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"

PG_MODULE_MAGIC;

/* Report type enumeration */
typedef enum ReportType {
    REPORT_TYPE_TEXT,
    REPORT_TYPE_CHART,
    REPORT_TYPE_TABLE,
    REPORT_TYPE_MIXED
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

// Generate report function
char* generate_report(const char *description)
{
    return generate_typed_report(description, REPORT_CUSTOM);
}

// Generate report by specified type
char* generate_typed_report(const char *description, ReportType type)
{
    if (!description) {
        return pstrdup("Report description cannot be empty");
    }
    
    const char *system_message;
    
    // Set system message based on report type
    switch (type) {
        case REPORT_TREND:
            system_message = "Generate trend analysis SQL. You need to generate SQL queries for time series data.";
            break;
        case REPORT_COMPARISON:
            system_message = "Generate comparison analysis SQL. You need to compare data across different categories or time periods.";
            break;
        case REPORT_SUMMARY:
            system_message = "Generate summary analysis SQL. You need to aggregate data and generate summary statistics.";
            break;
        case REPORT_FORECAST:
            system_message = "Generate forecast analysis SQL. You need to predict future trends based on historical data.";
            break;
        case REPORT_CUSTOM:
        default:
            system_message = "You are a data analysis expert. Based on the user's natural language description, "
                            "generate PostgreSQL SQL queries. The queries should be clear, efficient, and meet user requirements. "
                            "Only return SQL queries, do not add other explanations.";
            break;
    }
    
    // Construct prompt
    char *prompt = palloc(strlen(description) + 200);
    sprintf(prompt, "Generate PostgreSQL SQL query based on the following description:\n\n%s\n\n"
                  "Please return only SQL query, do not include other explanations.", description);
    
    // Call LLM to generate SQL
    ModelResponse *response = call_model("chatgpt", prompt, system_message);
    
    if (!response->successful) {
        pfree(prompt);
        pfree(response);
        return pstrdup("Unable to generate report SQL");
    }
    
    // Execute generated SQL and return results
    char *result = execute_report_sql(response->response);
    
    pfree(prompt);
    pfree(response);
    
    return result;
}

// Execute report SQL and generate results
char* execute_report_sql(const char *sql)
{
    if (!sql) {
        return pstrdup("SQL is empty");
    }
    
    int ret;
    char *result = NULL;
    
    // Connect to SPI
    SPI_connect();
    
    // Execute SQL
    ret = SPI_execute(sql, true, 0);
    
    if (ret != SPI_OK_SELECT) {
        SPI_finish();
        return pstrdup("SQL execution error");
    }
    
    // Process results
    if (SPI_processed > 0) {
        // Convert results to JSON format
        StringInfoData json_result;
        initStringInfo(&json_result);
        
        // Get column names
        TupleDesc tupdesc = SPI_tuptable->tupdesc;
        int ncolumns = tupdesc->natts;
        
        appendStringInfo(&json_result, "{\"columns\":[");
        for (int i = 0; i < ncolumns; i++) {
            appendStringInfo(&json_result, "\"%s\"%s", 
                           NameStr(tupdesc->attrs[i]->attname),
                           (i < ncolumns - 1) ? "," : "");
        }
        appendStringInfo(&json_result, "],\"rows\":[");
        
        // Get data rows
        for (int i = 0; i < SPI_processed; i++) {
            appendStringInfo(&json_result, "[");
            for (int j = 0; j < ncolumns; j++) {
                char *value = SPI_getvalue(SPI_tuptable->vals[i], tupdesc, j + 1);
                appendStringInfo(&json_result, "\"%s\"%s", 
                               value ? value : "null",
                               (j < ncolumns - 1) ? "," : "");
                if (value) pfree(value);
            }
            appendStringInfo(&json_result, "]%s", (i < SPI_processed - 1) ? "," : "");
        }
        appendStringInfo(&json_result, "]}");
        
        result = pstrdup(json_result.data);
        pfree(json_result.data);
    } else {
        result = pstrdup("{\"columns\":[],\"rows\":[]}");
    }
    
    // Disconnect from SPI
    SPI_finish();
    
    return result;
}

/* Generate report */
Datum pg_llm_generate_report(PG_FUNCTION_ARGS)
{
    text *query = PG_GETARG_TEXT_P(0);
    text *config = PG_GETARG_TEXT_P(1);
    
    // Parse configuration
    Jsonb *config_json = DatumGetJsonb(DirectFunctionCall1(jsonb_in, CStringGetDatum(text_to_cstring(config))));
    
    // Execute query to get data
    char *sql_result = execute_report_sql(text_to_cstring(query));
    
    // Generate report content
    StringInfoData result;
    initStringInfo(&result);
    
    // Generate unique ID
    char report_id[32];
    snprintf(report_id, sizeof(report_id), "report_%ld", GetCurrentTimestamp());
    
    // Generate different types of reports based on configuration
    if (config_json->include_chart) {
        // Generate chart
        appendStringInfo(&result, "{\n");
        appendStringInfo(&result, "  \"type\": \"report\",\n");
        appendStringInfo(&result, "  \"id\": \"%s\",\n", report_id);
        appendStringInfo(&result, "  \"title\": \"%s\",\n", config_json->title);
        appendStringInfo(&result, "  \"chart\": {\n");
        appendStringInfo(&result, "    \"type\": \"%s\",\n", config_json->chart_type);
        appendStringInfo(&result, "    \"data\": %s\n", sql_result);
        appendStringInfo(&result, "  },\n");
        appendStringInfo(&result, "  \"visualization_url\": \"http://localhost:8080/visualization/%s\",\n", report_id);
        appendStringInfo(&result, "  \"table\": %s\n", sql_result);
        appendStringInfo(&result, "}\n");
    } else {
        // Generate text report
        appendStringInfo(&result, "{\n");
        appendStringInfo(&result, "  \"type\": \"report\",\n");
        appendStringInfo(&result, "  \"id\": \"%s\",\n", report_id);
        appendStringInfo(&result, "  \"title\": \"%s\",\n", config_json->title);
        appendStringInfo(&result, "  \"content\": %s\n", sql_result);
        appendStringInfo(&result, "}\n");
    }
    
    pfree(sql_result);
    
    PG_RETURN_TEXT_P(cstring_to_text(result.data));
}

/* Get visualization URL */
Datum pg_llm_get_visualization_url(PG_FUNCTION_ARGS)
{
    text *report_id = PG_GETARG_TEXT_P(0);
    
    // Generate visualization URL
    StringInfoData url;
    initStringInfo(&url);
    appendStringInfo(&url, "http://localhost:8080/visualization/%s", text_to_cstring(report_id));
    
    PG_RETURN_TEXT_P(cstring_to_text(url.data));
} 