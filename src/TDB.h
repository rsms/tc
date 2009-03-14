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

#define tc_TDB_CheckExact(op) (Py_TYPE(op) == &tc_TDBType)
#define tc_TDB_Check(op) \
  ((Py_TYPE(op) == &tc_TDBType) || PyObject_TypeCheck((PyObject *)(op), &tc_TDBType))

#endif
