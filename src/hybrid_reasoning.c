#include "../include/hybrid_reasoning.h"
#include "../include/model_interface.h"
#include <string.h>

// Configuration
static float confidence_threshold = 0.7;
static char **parallel_models = NULL;
static int parallel_model_count = 0;
static char *fallback_strategy = NULL;

// Hybrid reasoning query
char* hybrid_reasoning_query(const char *query, const char *preferred_model)
{
    ModelResponse **responses = NULL;
    int response_count = 0;
    
    // Whether to use a single specified model
    if (strcmp(preferred_model, "auto") != 0) {
        responses = palloc(sizeof(ModelResponse*));
        responses[0] = call_model(preferred_model, query, NULL);
        response_count = 1;
    } else {
        // Call multiple models in parallel
        responses = palloc(sizeof(ModelResponse*) * parallel_model_count);
        response_count = parallel_model_count;
        
        for (int i = 0; i < parallel_model_count; i++) {
            responses[i] = call_model(parallel_models[i], query, NULL);
        }
    }
    
    // Find response with highest confidence
    int best_index = 0;
    float highest_confidence = 0;
    
    for (int i = 0; i < response_count; i++) {
        if (responses[i]->successful && responses[i]->confidence > highest_confidence) {
            highest_confidence = responses[i]->confidence;
            best_index = i;
        }
    }
    
    // Check confidence threshold
    if (highest_confidence < confidence_threshold) {
        // Fall back to local model
        ModelResponse *local_response = call_local_model(query, NULL);
        char *result = pstrdup(local_response->response);
        pfree(local_response);
        
        // Clean up resources
        for (int i = 0; i < response_count; i++) {
            pfree(responses[i]);
        }
        pfree(responses);
        
        return result;
    }
    
    // Use response with highest confidence
    char *result = pstrdup(responses[best_index]->response);
    
    // Clean up resources
    for (int i = 0; i < response_count; i++) {
        pfree(responses[i]);
    }
    pfree(responses);
    
    return result;
}

// Set confidence threshold
bool set_confidence_threshold(float threshold)
{
    if (threshold < 0 || threshold > 1) {
        return false;
    }
    
    confidence_threshold = threshold;
    return true;
}

// Set models for parallel calls
bool set_parallel_models(const char **model_names, int count)
{
    // Clean up old configuration
    if (parallel_models) {
        for (int i = 0; i < parallel_model_count; i++) {
            pfree(parallel_models[i]);
        }
        pfree(parallel_models);
    }
    
    // Set new configuration
    parallel_models = palloc(sizeof(char*) * count);
    parallel_model_count = count;
    
    for (int i = 0; i < count; i++) {
        parallel_models[i] = pstrdup(model_names[i]);
    }
    
    return true;
} 