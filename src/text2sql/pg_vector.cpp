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
  Vector* result = (Vector*)palloc0(VECTOR_SIZE(vec.size()));
  SET_VECTOR_DIM(result, vec.size());
  memcpy(VECTOR_DATA(result), vec.data(), vec.size() * sizeof(float));
  return PointerGetDatum(result);
}
