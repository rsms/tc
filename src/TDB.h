#ifndef PYTC_TDB_H
#define PYTC_TDB_H

#include "_base.h"
#include <tctdb.h>

typedef struct {
  PyObject_HEAD
  TCTDB	*db;
} tc_TDB;

extern PyTypeObject tc_TDBType;

int tc_TDB_register(PyObject *module);

#endif
