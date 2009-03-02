#ifndef PYTC_BDBCURSOR_H
#define PYTC_BDBCURSOR_H

#include "_base.h"
#include <tcbdb.h>
#include "BDB.h"

typedef struct {
  PyObject_HEAD
  tc_BDB *bdb;
  BDBCUR *cur;
  tc_itertype_t itype;
} tc_BDBCursor;

extern PyTypeObject tc_BDBCursorType;

PyObject *tc_BDBCursor_new(PyTypeObject *type, PyObject *args, PyObject *keywds);
void tc_BDBCursor_dealloc(tc_BDBCursor *self);
PyObject *tc_BDBCursor_first(tc_BDBCursor *self);

int tc_BDBCursor_register(PyObject *module);

#endif
