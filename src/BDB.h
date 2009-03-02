#ifndef PYTC_BDB_H
#define PYTC_BDB_H

#include "_base.h"
#include <tcbdb.h>

typedef struct {
  PyObject_HEAD
  TCBDB	*bdb;
  PyObject *cmp;
  PyObject *cmpop;
} tc_BDB;

extern PyTypeObject tc_BDBType;

int tc_BDB_register(PyObject *module);


/* utils */
void tc_Error_SetBDB(TCBDB *bdb);

#endif
