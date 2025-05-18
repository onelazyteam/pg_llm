#include "text2sql/pg_vector.h"

static Oid pg_llm_model_relid = InvalidOid;

Oid get_vector_type_oid() {
  if (unlikely(!OidIsValid(pg_llm_model_relid))) {
    TypeName *typeName = makeTypeNameFromNameList(list_make1(makeString(pstrdup("vector"))));
    pg_llm_model_relid = typenameTypeId(NULL, typeName);
  }
  return pg_llm_model_relid;
}

// Helper function to convert std::vector to PostgreSQL vector
Datum std_vector_to_vector(const std::vector<float>& vec) {
  // Check if vector is empty
  if (vec.empty()) {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                   errmsg("Cannot convert empty vector")));
  }

  // Check if vector dimension exceeds limit
  if (vec.size() > VECTOR_MAX_DIM) {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                   errmsg("Vector dimension %zu exceeds limit %d",
                         vec.size(), VECTOR_MAX_DIM)));
  }

  // Calculate size and allocate memory
  size_t dim = vec.size();
  size_t size = VECTOR_SIZE(dim);
  Vector *result = (Vector *) palloc0(size);

  // Initialize vector header
  SET_VARSIZE(result, size);
  result->dim = dim;

  // Copy vector data directly
  memcpy(VECTOR_DATA(result), vec.data(), dim * sizeof(float));

  // Return the new vector
  PG_RETURN_POINTER(result);
}
