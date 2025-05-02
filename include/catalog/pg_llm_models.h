#pragma once

extern "C" {
#include "postgres.h"  // clang-format off
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "postgres_ext.h"
#include "storage/pg_shmem.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

#define Natts_pg_llm_models 5
#define Anum_pg_llm_local_model 1
#define Anum_pg_llm_model_type 2
#define Anum_pg_llm_instance_name 3
#define Anum_pg_llm_api_key 4
#define Anum_pg_llm_config 5

void pg_llm_model_insert(bool local_model, char* model_type, char* instance_name, char* api_key, char* config);
void pg_llm_model_delete(char* instance_name);
}

#include <string>
#include <vector>

bool pg_llm_model_get_infos(bool *local_model,
                            const std::string instance_name,
                            std::string &model_type,
                            std::string &api_key,
                            std::string &config);

std::vector<std::string> pg_llm_get_all_instancenames();