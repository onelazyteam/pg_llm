#ifndef HYBRID_REASONING_H
#define HYBRID_REASONING_H

#include "postgres.h"
#include "model_interface.h"

// Hybrid reasoning result
typedef struct {
    char *response;
    char *selected_model;
    float confidence;
    bool fallback_used;
} HybridResult;

// Hybrid reasoning function
char* hybrid_reasoning_query(const char *query, const char *preferred_model);

// Strategy settings
bool set_confidence_threshold(float threshold);
bool set_parallel_models(const char **model_names, int count);
bool set_fallback_strategy(const char *strategy);

#endif /* HYBRID_REASONING_H */ 