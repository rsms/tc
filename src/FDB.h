#ifndef PYTC_FDB_H
#define PYTC_FDB_H

#include "_base.h"
#include <tcfdb.h>

typedef struct _tcfdbobject TcfdbObject;
struct _tcfdbobject {
        PyObject_HEAD
        TCFDB *db;
};

int tcfdb_register(PyObject *module);

#endif
