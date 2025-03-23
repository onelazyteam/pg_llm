#include <vector>

extern "C" {
  #include "postgres.h"  // clang-format off
  #include "vector.h"
  #include "funcapi.h"
  #include "nodes/makefuncs.h"
  #include "parser/parse_type.h"
}

// Vector dimension
#define VECTOR_DIM(vector) (((Vector *) (vector))->dim)

// Vector data
#define VECTOR_DATA(vector) (((Vector *) (vector))->x)

// Set vector dimension
#define SET_VECTOR_DIM(vector, new_dim) \
  do { \
    ((Vector *) (vector))->dim = (new_dim); \
  } while (0)

Oid get_vector_type_oid();

Datum std_vector_to_vector(const std::vector<float>& vec);
