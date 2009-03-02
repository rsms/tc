#ifndef PYTC_HDB_H
#define PYTC_HDB_H

#include "_base.h"
#include <tchdb.h>

typedef struct {
  PyObject_HEAD
  TCHDB	*hdb;
  tc_itertype_t itype;
  bool hold_itype;
} tc_HDB;

extern PyTypeObject tc_HDBType;

int tc_HDB_register(PyObject *module);

#endif
