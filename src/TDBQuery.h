#ifndef PYTC_TDBQUERY_H
#define PYTC_TDBQUERY_H

#include "_base.h"
#include <tctdb.h>

typedef struct {
  PyObject_HEAD
  TDBQRY *qry;
} tc_TDBQuery;

extern PyTypeObject tc_TDBQueryType;

int tc_TDBQuery_register(PyObject *module);

tc_TDBQuery *tc_TDBQuery_new_capi(TCTDB *tdb);

#endif
