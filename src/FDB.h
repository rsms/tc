#ifndef PYTC_FDB_H
#define PYTC_FDB_H

#include "_base.h"
#include <tcfdb.h>

typedef struct {
  PyObject_HEAD
  TCFDB	*db;
  tc_itertype_t itype;
  bool hold_itype;
} tc_FDB;

extern PyTypeObject tc_FDBType;

int tc_FDB_register(PyObject *module);

#endif
