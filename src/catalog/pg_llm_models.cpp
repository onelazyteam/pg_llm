#include "catalog/pg_llm_models.h"

#include <string>

static Oid pg_llm_model_relid = InvalidOid;

static inline void initialize_pg_llm_relid() {
  if (unlikely(!OidIsValid(pg_llm_model_relid))) {
    pg_llm_model_relid = get_relname_relid(
                           "pg_llm_models", get_namespace_oid("_pg_llm_catalog", false));
  }
}

void pg_llm_model_insert(bool local_model, char *model_type, char *instance_name, char *api_key, char *config) {
  initialize_pg_llm_relid();
  Relation rel = table_open(pg_llm_model_relid, RowExclusiveLock);

  TupleDesc tupdesc = RelationGetDescr(rel);

  /* Prepare arrays for values and null flags */
  Datum    values[Natts_pg_llm_models];
  bool     nulls[Natts_pg_llm_models] = {false, false, false, false, false};

  values[0] = BoolGetDatum(local_model);
  values[1] = CStringGetTextDatum(model_type);
  values[2] = CStringGetTextDatum(instance_name);
  values[3] = CStringGetTextDatum(api_key);
  values[4] = CStringGetTextDatum(config);

  tupdesc = RelationGetDescr(rel);
  HeapTuple tuple = heap_form_tuple(tupdesc, values, nulls);

  CatalogTupleInsert(rel, tuple);

  heap_freetuple(tuple);
  table_close(rel, RowExclusiveLock);
}

void pg_llm_model_delete(char *instance_name) {
  initialize_pg_llm_relid();

  Relation rel = table_open(pg_llm_model_relid, RowExclusiveLock);
  Snapshot snapshot = GetTransactionSnapshot();
  TableScanDesc scan = table_beginscan(rel, snapshot, 0, NULL);
  HeapTuple  tup;
  while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL) {
    Datum dname;
    bool isnull;
    char *cname;

    /* Extract instance_name column (second column, attnum = 2) */
    dname = heap_getattr(tup, Anum_pg_llm_instance_name, RelationGetDescr(rel), &isnull);
    if (!isnull) {
      cname = TextDatumGetCString(dname);
      if (strcmp(cname, instance_name) == 0) {
        simple_heap_delete(rel, &tup->t_self);
      }
    }
  }

  /* End scan and close relation */
  heap_endscan(scan);
  table_close(rel, RowExclusiveLock);
}

bool pg_llm_model_get_infos(bool *local_model,
                            const std::string instance_name,
                            std::string &model_type,
                            std::string &api_key,
                            std::string &config) {
  bool result = false;
  initialize_pg_llm_relid();

  Relation rel = table_open(pg_llm_model_relid, AccessShareLock);

  ScanKeyData scankey;
  ScanKeyInit(&scankey, Anum_pg_llm_instance_name, BTEqualStrategyNumber,
              F_TEXTEQ, CStringGetTextDatum(instance_name.c_str()));
  Snapshot snapshot = GetTransactionSnapshot();
  TableScanDesc scan = table_beginscan_strat(rel, snapshot, 1, &scankey, true, true);

  HeapTuple  tup;
  while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL) {
    Datum dname;
    bool isnull;
    /* Extract instance_name column (second column, attnum = 2) */
    dname = heap_getattr(tup, Anum_pg_llm_instance_name, RelationGetDescr(rel), &isnull);
    if (!isnull) {
      dname = heap_getattr(tup, Anum_pg_llm_local_model, RelationGetDescr(rel), &isnull);
      *local_model = DatumGetBool(dname);
      dname = heap_getattr(tup, Anum_pg_llm_model_type, RelationGetDescr(rel), &isnull);
      model_type = std::string(TextDatumGetCString(dname));
      dname = heap_getattr(tup, Anum_pg_llm_api_key, RelationGetDescr(rel), &isnull);
      api_key = std::string(TextDatumGetCString(dname));
      dname = heap_getattr(tup, Anum_pg_llm_config, RelationGetDescr(rel), &isnull);
      config = std::string(TextDatumGetCString(dname));
      result = true;
    }
  }

  /* End scan and close relation */
  heap_endscan(scan);
  table_close(rel, AccessShareLock);
  return result;
}

std::vector<std::string> pg_llm_get_all_instancenames() {
  Relation    rel;
  TableScanDesc scan;
  HeapTuple   tup;
  TupleDesc   tupdesc;
  bool        isnull;
  std::vector<std::string> names;

  initialize_pg_llm_relid();

  rel = table_open(pg_llm_model_relid, AccessShareLock);

  scan = table_beginscan_catalog(rel, 0, NULL);

  tupdesc = RelationGetDescr(rel);

  while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL) {
    Datum d = heap_getattr(tup,
                           Anum_pg_llm_instance_name,
                           tupdesc,
                           &isnull);
    if (!isnull) {
      std::string s = std::string(TextDatumGetCString(d));
      names.push_back(s);
    }
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);

  return names;
}
