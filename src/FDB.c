#include "FDB.h"
#include "util.h"

/* Private --------------------------------------------------------------- */

/* Sets the error message */
static void tc_Error_SetFDB(TCFDB *db) {
    int ecode = tcfdbecode(db);
    if (ecode == TCENOREC) {
        PyErr_SetString(PyExc_KeyError, tcfdberrmsg(ecode));
    } else {
        tc_Error_SetCodeAndString(ecode, tcfdberrmsg(ecode));
    }
}

/* Used by tc_FDB_get and tc_FDB___getitem__ */
static PyObject *tc_FDB_intern_get(tc_FDB *self, PY_LONG_LONG key) {
    PyObject *ret;
    char *value;
    int value_len;

    Py_BEGIN_ALLOW_THREADS
    value = tcfdbget(self->db, key, &value_len);
    Py_END_ALLOW_THREADS

    if (!value) {
        tc_Error_SetFDB(self->db);
        return NULL;
    }

    ret = PyBytes_FromStringAndSize(value, value_len);
    free(value);
    return ret;
}

/* Used for Tune and Optimize */
#define TC_FDB_TUNE_OPTIMIZE(func, method, call) \
static PyObject * func(tc_FDB *self, PyObject *args, PyObject *keywds) { \
    bool result; \
    int width; \
    PY_LONG_LONG limsiz; \
    static char *kwlist[] = {"width", "limsiz", NULL}; \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "iL:" #method, kwlist, &width, &limsiz)) { \
        PyErr_SetString(PyExc_TypeError, "Something is wrong with the arguments"); \
        return NULL; \
    } \
\
    Py_BEGIN_ALLOW_THREADS \
    result = call(self->db, width, limsiz); \
    Py_END_ALLOW_THREADS \
\
    if (!result) { \
        tc_Error_SetFDB(self->db); \
        return NULL; \
    } \
    Py_RETURN_NONE; \
}

/* Used for Put, Putkeep and Putcat */
#define TC_FDB_PUT(func, method, call) \
static PyObject * func(tc_FDB *self, PyObject *args, PyObject *keywds) { \
    bool result; \
    PY_LONG_LONG key; \
    char *value; \
    int value_len; \
    static char *kwlist[] = {"key", "value", NULL}; \
\
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "Ls#:" #method, kwlist, \
                                     &key, &value, &value_len)) { \
        PyErr_SetString(PyExc_TypeError, "Something is wrong with the arguments"); \
        return NULL; \
    } \
\
    Py_BEGIN_ALLOW_THREADS \
    result = call(self->db, key, value, value_len); \
    Py_END_ALLOW_THREADS \
\
    if (!result) { \
        tc_Error_SetFDB(self->db); \
        return NULL; \
    } \
    Py_RETURN_NONE; \
}

#define TC_INT_LLKEYARGS(func,type,method,call,member,error) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    PY_LONG_LONG key; \
    int ret; \
    static char *kwlist[] = {"key", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "L:" #method, kwlist, \
                                     &key)) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    ret = call(self->member, key); \
    Py_END_ALLOW_THREADS \
  \
    if (ret == -1) { \
      error(self->member); \
      return NULL; \
    } \
    return NUMBER_FromLong((long)ret); \
  }

/* NOTE: this function dealloc pointer returned by tc */
/*
#define TC_STRINGL_LLKEYARGS(func,type,method,call,member,error) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    PyObject *ret; \
    PY_LONG_LONG key; \
    char *value; \
    int value_len; \
    static char *kwlist[] = {"key", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "L:" #method, kwlist, \
                                     &key)) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    value = call(self->member, key, &value_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!value) { \
      error(self->member); \
      return NULL; \
    } \
    ret = PyBytes_FromStringAndSize(value, value_len); \
    free(value); \
    return ret; \
  }
*/

/* Public --------------------------------------------------------------- */

/**
 * tc.FDB.ecode()
 * look at tcfdbecode
 */
static PyObject *tc_FDB_ecode(tc_FDB *self) {
    return NUMBER_FromLong((long)tcfdbecode(self->db));
}

/**
 * tc.FDB.errmsg(ecode)
 * look at tcfdberrmsg
 */
static PyObject *tc_FDB_errmsg(PyTypeObject *type, PyObject *args, PyObject *keywds) {
    int ecode;
    static char *kwlist[] = {"ecode", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "i:errmsg", kwlist, &ecode))
        return NULL;

    return PyBytes_FromString(tcfdberrmsg(ecode));
}

/**
 * tc.FDB.__del__()
 * look at tcfdbdel
 */
static void tc_FDB_dealloc(tc_FDB *self) {
    if (self->db) {
        Py_BEGIN_ALLOW_THREADS
        tcfdbdel(self->db);
        Py_END_ALLOW_THREADS
    }
    PyObject_Del(self);
}

/**
 * tc.FDB.__new__()
 * look at tcfdbnew and tcfdbopen
 */
static PyObject *tc_FDB_new(PyTypeObject *type, PyObject *args, PyObject *keywds) {
    tc_FDB *self;
    if (!(self = (tc_FDB *)type->tp_alloc(type, 0))) {
        PyErr_SetString(PyExc_MemoryError, "Cannot alloc tc_FDB instance");
        return NULL;
    }

    if ((self->db = tcfdbnew())) {
        int omode = 0;
        char *path = NULL;
        static char *kwlist[] = {"path", "omode", NULL};

        if (PyArg_ParseTupleAndKeywords(args, keywds, "|si:open", kwlist, &path, &omode)) {
            if (path && omode) {
                bool result;
                Py_BEGIN_ALLOW_THREADS
                result = tcfdbopen(self->db, path, omode);
                Py_END_ALLOW_THREADS
                if (result) {
                    return (PyObject *)self;
                }
                tc_Error_SetFDB(self->db);
            } else {
                return (PyObject *)self;
            }
        }
    } else {
        PyErr_SetString(PyExc_MemoryError, "Cannot alloc TCFDB instance");
    }
    tc_FDB_dealloc(self);
    return NULL;
}

/**
 * tc.FDB.__hash__()
 * not supported
 */
static long tc_FDB_hash(PyObject *self) {
    PyErr_SetString(PyExc_TypeError, "FDB objects are unhashable");
    return -1L;
}

/**
 * tc.FDB.tune(width, limsiz)
 * look at tcfdbtune
 */
TC_FDB_TUNE_OPTIMIZE(tc_FDB_tune, tune, tcfdbtune);

/**
 * tc.FDB.optimize(width, limsiz)
 * look at tcfdboptimize
 */
TC_FDB_TUNE_OPTIMIZE(tc_FDB_optimize, optimize, tcfdboptimize);

/**
 * tc.FDB.put(key, value)
 * look at tcfdbput
 */
TC_FDB_PUT(tc_FDB_put, put, tcfdbput);

/**
 * tc.FDB.putkeep(key, value)
 * look at tcfdbputkeep
 */
TC_FDB_PUT(tc_FDB_putkeep, putkeep, tcfdbputkeep);

/**
 * tc.FDB.putcat(key, value)
 * look at tcfdbputcat
 */
TC_FDB_PUT(tc_FDB_putcat, putcat, tcfdbputcat);

/**
 * tc.FDB.get(key)
 * look at tcfdbget
 */
static PyObject *tc_FDB_get(tc_FDB *self, PyObject *args) {
    PY_LONG_LONG key;
    if (!PyArg_ParseTuple(args, "L:get", &key)) {
        PyErr_SetString(PyExc_TypeError, "Argument key is of wrong type");
        return NULL;
    }

    return tc_FDB_intern_get(self, key);
}

/**
 * tc.FDB.__getitem__(key)
 * look at tcfdbget
 */
static PyObject *tc_FDB___getitem__(tc_FDB *self, PyObject *_key) {
    PY_LONG_LONG key = PyLong_AsLongLong(_key);

    if (PyErr_ExceptionMatches(PyExc_TypeError)) {
        PyErr_SetString(PyExc_TypeError, "Argument key is of wrong type");
        return NULL;
    }

    return tc_FDB_intern_get(self, key);
}

/**
 * tc.FDB.__setitem__(key)
 * look at tcfdbput and tcfdbout
 */
static int tc_FDB___setitem__(tc_FDB *self, PyObject *_key, PyObject *_value) {
    bool result;

    PY_LONG_LONG key = PyLong_AsLongLong(_key);
    
    if (PyErr_ExceptionMatches(PyExc_TypeError)) {
        PyErr_Clear();
        PyErr_SetString(PyExc_TypeError, "The key is not an integer");
        return -1;
    }

    if (_value == NULL) {
        Py_BEGIN_ALLOW_THREADS
        result = tcfdbout(self->db, key);
        Py_END_ALLOW_THREADS

    } else {
        char *value = PyBytes_AsString(_value);
        int value_len = PyBytes_GET_SIZE(_value);
    
        if (!value || !value_len) {
            PyErr_SetString(PyExc_TypeError, "The value must be a string");
            return -1;
        }

        Py_BEGIN_ALLOW_THREADS
        result = tcfdbput(self->db, key, value, value_len);
        Py_END_ALLOW_THREADS
    }
    if (!result) {
        tc_Error_SetFDB(self->db);
        return -1;
    }
    return 0;
}

/**
 * tc.FDB.out(key)
 * look at tcfdbout
 */
static PyObject *tc_FDB_out(tc_FDB *self, PyObject *args) {
    bool result;
    PY_LONG_LONG key;
    if (!PyArg_ParseTuple(args, "L:get", &key)) {
        PyErr_SetString(PyExc_TypeError, "Argument key is of wrong type");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    result = tcfdbout(self->db, key);
    Py_END_ALLOW_THREADS

    if (!result) {
        tc_Error_SetFDB(self->db);
        return NULL;
    }
    return PyBool_FromLong(true);
}

/**
 * tc.FDB.setmutex()
 * look at tcfdbsetmutex
 */
TC_BOOL_NOARGS(tc_FDB_setmutex,tc_FDB,tcfdbsetmutex,db,tc_Error_SetFDB,db);

/**
 * tc.FDB.close()
 * look at tcfdbclose
 */
TC_BOOL_NOARGS(tc_FDB_close,tc_FDB,tcfdbclose,db,tc_Error_SetFDB,db);

/**
 * tc.FDB.iterinit()
 * look at tcfdbiterinit
 */
TC_BOOL_NOARGS(tc_FDB_iterinit,tc_FDB,tcfdbiterinit,db,tc_Error_SetFDB,db);

/**
 * tc.FDB.sync()
 * look at tcfdbsync
 */
TC_BOOL_NOARGS(tc_FDB_sync,tc_FDB,tcfdbsync,db,tc_Error_SetFDB,db);

/**
 * tc.FDB.path()
 * look at tcfdbpath
 */
TC_STRING_NOARGS(tc_FDB_path,tc_FDB,tcfdbpath,db,tc_Error_SetFDB);

/**
 * tc.FDB.vanish()
 * look at tcfdbvanish
 */
TC_BOOL_NOARGS(tc_FDB_vanish,tc_FDB,tcfdbvanish,db,tc_Error_SetFDB,db);

/**
 * tc.FDB.copy(path)
 * look at tcfdbcopy
 */
TC_BOOL_PATHARGS(tc_FDB_copy, tc_FDB, copy, tcfdbcopy, db, tc_Error_SetFDB);

TC_XDB_OPEN(tc_FDB_open,tc_FDB,tc_FDB_new,tcfdbopen,db,tc_FDB_dealloc,tc_Error_SetFDB);

TC_INT_LLKEYARGS(tc_FDB_vsiz,tc_FDB,vsiz,tcfdbvsiz,db,tc_Error_SetFDB);

static PyObject *tc_FDB_GetIter(tc_FDB *self, tc_itertype_t itype) {
    if (tc_FDB_iterinit(self)) {
        Py_INCREF(self);
        /* hack */
        if (self->hold_itype) {
            self->hold_itype = false;
        } else {
            self->itype = itype;
            if (itype != tc_iter_key_t) {
                self->hold_itype = true;
            }
        }
        return (PyObject *)self;
    }
    return NULL;
}

TC_XDB_iters(tc_FDB,tc_FDB_GetIter_keys,tc_FDB_GetIter_values,tc_FDB_GetIter_items,tc_FDB_GetIter);

static PyObject *tc_FDB_iternext(tc_FDB *self) {
  log_trace("ENTER");
  PyObject *ret = NULL; \
  if (self->itype == tc_iter_key_t) {
    PY_LONG_LONG key;

    Py_BEGIN_ALLOW_THREADS
    key = tcfdbiternext(self->db);
    Py_END_ALLOW_THREADS

    if (key == 0)
        ret = Py_BuildValue("");

    ret = Py_BuildValue("L", key);
  } else {
    // Should implement itervalue and iteritems here
    ret = Py_BuildValue("");
    /*
    TC_GET_TCXSTR_KEY_VALUE(tcfdbiternext3,self->db)
    if (result) {
      if (self->itype == tc_iter_value_t) {
        ret = PyBytes_FromStringAndSize(tcxstrptr(value), tcxstrsize(value));
      } else {
        ret = Py_BuildValue("(s#s#)", tcxstrptr(key), tcxstrsize(key),
                                      tcxstrptr(value), tcxstrsize(value));
      }
    }
    TC_CLEAR_TCXSTR_KEY_VALUE()
    */
  }

  return ret;
}

TC_U_LONG_LONG_NOARGS(tc_FDB_rnum, tc_FDB, tcfdbrnum, db, tcfdbecode, tc_Error_SetFDB);
TC_U_LONG_LONG_NOARGS(tc_FDB_fsiz, tc_FDB, tcfdbrnum, db, tcfdbecode, tc_Error_SetFDB);
/* todo: features for experts */

static int tc_FDB_contains(tc_FDB *self, PyObject *args) {
    PY_LONG_LONG key;
    int value_len;

    if (!PyArg_ParseTuple(args, "L:contains", &key))
        return 0;

    Py_BEGIN_ALLOW_THREADS
    value_len = tcfdbvsiz(self->db, key);
    Py_END_ALLOW_THREADS

    return (value_len != -1);
}

TC_XDB___contains__(tc_FDB___contains__,tc_FDB,tc_FDB_Contains);

// TC_XDB___getitem__(tc_FDB___getitem__,tc_FDB,tcfdbget,db,tc_Error_SetFDB);
TC_XDB_rnum(TCFDB_rnum,TCFDB,tcfdbrnum);

static PyObject *tc_FDB_keys(tc_FDB *self) {
    log_trace("ENTER");
    int i;
    PyObject *ret;
    if (!tc_FDB_iterinit((tc_FDB *)self) ||
        !(ret = PyList_New(TCFDB_rnum(self->db)))) {
        return NULL;
    }
    for (i = 0; ; i++) {
        PY_LONG_LONG key;
        PyObject *_key;

        Py_BEGIN_ALLOW_THREADS
        key = tcfdbiternext(self->db);
        Py_END_ALLOW_THREADS

        if (!key)
            break;

        _key = Py_BuildValue("L", key);
        if (!_key) {
            Py_DECREF(ret);
            return NULL;
        }
        PyList_SET_ITEM(ret, i, _key);
    }
    return ret;
}

/*
static PyObject *tc_FDB_items(tc_FDB *self) {
  log_trace("ENTER");
  int i, n = TCFDB_rnum(self->db);
  PyObject *ret, *item;
  if (!tc_FDB_iterinit((tc_FDB *)self) ||
      !(ret = PyList_New(n))) {
    return NULL;
  }
  for (i = 0; i < n; i++) {
    if (!(item = PyTuple_New(2))) {
      Py_DECREF(ret);
      return NULL;
    }
    PyList_SET_ITEM(ret, i, item);
  }
  for (i = 0; ; i++) {
    char *key, *value;
    int key_len, value_len;

    Py_BEGIN_ALLOW_THREADS
    key = tcfdbiternext(self->db, &key_len);
    Py_END_ALLOW_THREADS
    if (!key) { break; }

    Py_BEGIN_ALLOW_THREADS
    value = tcfdbget(self->db, key, key_len, &value_len);
    Py_END_ALLOW_THREADS

    if (value) {
      PyObject *_key, *_value;
      _key = PyBytes_FromStringAndSize(key, key_len);
      free(key);
      if (!_key) {
        Py_DECREF(ret);
        return NULL;
      }
      _value = PyBytes_FromStringAndSize(value, value_len);
      free(value);
      if (!_value) {
        Py_DECREF(_key);
        Py_DECREF(ret);
        return NULL;
      }
      item = PyList_GET_ITEM(ret, i);
      PyTuple_SET_ITEM(item, 0, _key);
      PyTuple_SET_ITEM(item, 1, _value);
    } else {
      free(key);
    }
  }
  return ret;
}
*/

/*
static PyObject *tc_FDB_values(tc_FDB *self) {
  log_trace("ENTER");
  int i;
  PyObject *ret;
  if (!tc_FDB_iterinit((tc_FDB *)self) ||
      !(ret = PyList_New(TCFDB_rnum(self->db)))) {
    return NULL;
  }
  for (i = 0; ; i++) {
    char *key, *value;
    int key_len, value_len;

    Py_BEGIN_ALLOW_THREADS
    key = tcfdbiternext(self->db, &key_len);
    Py_END_ALLOW_THREADS

    if (!key) { break; }

    Py_BEGIN_ALLOW_THREADS
    value = tcfdbget(self->db, key, key_len, &value_len);
    Py_END_ALLOW_THREADS
    free(key);

    if (value) {
      PyObject *_value;
      _value = PyBytes_FromStringAndSize(value, value_len);
      free(value);
      if (!_value) {
        Py_DECREF(ret);
        return NULL;
      }
      PyList_SET_ITEM(ret, i, _value);
    }
  }
  return ret;
}
*/

TC_XDB_length(tc_FDB_length,tc_FDB,TCFDB_rnum,db);
// TC_XDB_subscript(tc_FDB_subscript,tc_FDB,tcfdbget,db,tc_Error_SetFDB);

static int tc_FDB_DelItem(tc_FDB *self, PyObject *args) {
    PY_LONG_LONG key;
    bool result;

    if (!PyArg_ParseTuple(args, "L:delitem", &key)) {
        PyErr_SetString(PyExc_TypeError, "Argument key is of wrong type");
        return -1;
    }

    Py_BEGIN_ALLOW_THREADS
    result = tcfdbout(self->db, key);
    Py_END_ALLOW_THREADS

    if (!result) {
        tc_Error_SetFDB(self->db);
        return -1;
    }
    return 0;
}

/*
#define TC_XDB_SetItem(func,type,call,member,err) \
  int \
  func(type *self, PyObject *_key, PyObject *_value) { \
    bool result; \
    char *key = PyBytes_AsString(_key), *value = PyBytes_AsString(_value); \
    int key_len = PyBytes_GET_SIZE(_key), value_len = PyBytes_GET_SIZE(_value); \
  \
    if (!key || !key_len || !value) { \
      return -1; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    result = call(self->member, key, key_len, value, value_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!result) { \
      err(self->member); \
      return -1; \
    } \
    return 0; \
  }
*/

// TC_XDB_DelItem(tc_FDB_DelItem,tc_FDB,tcfdbout,db,tc_Error_SetFDB);
// TC_XDB_SetItem(tc_FDB_SetItem,tc_FDB,tcfdbput,db,tc_Error_SetFDB);
// TC_XDB_ass_sub(tc_FDB_ass_sub,tc_FDB,(int)tc_FDB___setitem__,tc_FDB_DelItem);
// TC_XDB_addint(tc_FDB_addint,tc_FDB,addint,tcfdbaddint,db,tc_Error_SetFDB);
// TC_XDB_adddouble(tc_FDB_adddouble,tc_FDB,addint,tcfdbadddouble,db,tc_Error_SetFDB);

/* methods of classes */
static PyMethodDef tc_FDB_methods[] = {
  {"errmsg", (PyCFunction)tc_FDB_errmsg, METH_VARARGS | METH_KEYWORDS | METH_CLASS,
    "Get the message string corresponding to an error code."},
  {"ecode", (PyCFunction)tc_FDB_ecode, METH_NOARGS,
    "Get the last happened error code of a fixed-length database object."},
  {"setmutex", (PyCFunction)tc_FDB_setmutex, METH_NOARGS,
    "Set mutual exclusion control of a fixed-length database object for threading."},
  {"tune", (PyCFunction)tc_FDB_tune, METH_VARARGS | METH_KEYWORDS,
    "Set the tuning parameters of a fixed-length database object."},
  {"open", (PyCFunction)tc_FDB_open, METH_VARARGS | METH_KEYWORDS,
    "Open a database file and connect a fixed-length database object."},
  {"close", (PyCFunction)tc_FDB_close, METH_NOARGS,
    "Close a fixed-length database object."},
  {"put", (PyCFunction)tc_FDB_put, METH_VARARGS | METH_KEYWORDS,
    "Store a record into a fixed-length database object."},
  {"putkeep", (PyCFunction)tc_FDB_putkeep, METH_VARARGS | METH_KEYWORDS,
    "Store a new record into a fixed-length database object."},
  {"putcat", (PyCFunction)tc_FDB_putcat, METH_VARARGS | METH_KEYWORDS,
    "Concatenate a value at the end of the existing record in a fixed-length database object."},
  {"out", (PyCFunction)tc_FDB_out, METH_VARARGS | METH_KEYWORDS,
    "Remove a record of a fixed-length database object."},
  {"get", (PyCFunction)tc_FDB_get, METH_VARARGS | METH_KEYWORDS,
    "Retrieve a record in a fixed-length database object."},
  {"vsiz", (PyCFunction)tc_FDB_vsiz, METH_VARARGS | METH_KEYWORDS,
    "Get the size of the value of a record in a fixed-length database object."},
  {"iterinit", (PyCFunction)tc_FDB_iterinit, METH_NOARGS,
    "Initialize the iterator of a fixed-length database object."},
  {"iternext", (PyCFunction)tc_FDB_iternext, METH_NOARGS,
    "Get the next extensible objects of the iterator of a fixed-length database object."},
  {"sync", (PyCFunction)tc_FDB_sync, METH_NOARGS,
    "Synchronize updated contents of a fixed-length database object with the file and the device."},
  {"optimize", (PyCFunction)tc_FDB_optimize, METH_VARARGS | METH_KEYWORDS,
    "Optimize the file of a fixed-length database object."},
  {"vanish", (PyCFunction)tc_FDB_vanish, METH_NOARGS,
    "Remove all records of a fixed-length database object."},
  {"path", (PyCFunction)tc_FDB_path, METH_NOARGS,
    "Get the file path of a fixed-length database object."},
  {"copy", (PyCFunction)tc_FDB_copy, METH_VARARGS | METH_KEYWORDS,
    "Copy the database file of a fixed-length database object."},
  {"rnum", (PyCFunction)tc_FDB_rnum, METH_NOARGS,
    "Get the number of records of a fixed-length database object."},
  {"fsiz", (PyCFunction)tc_FDB_fsiz, METH_NOARGS,
   "Get the size of the database file of a fixed-length database object."},
  {"__contains__", (PyCFunction)tc_FDB___contains__, METH_O | METH_COEXIST,
    NULL},
  {"__getitem__", (PyCFunction)tc_FDB___getitem__, METH_O | METH_COEXIST,
    NULL},
  {"has_key", (PyCFunction)tc_FDB___contains__, METH_O,
    NULL},
  {"keys", (PyCFunction)tc_FDB_keys, METH_NOARGS,
    NULL},
  // {"items", (PyCFunction)tc_FDB_items, METH_NOARGS,
  //  NULL},
  // {"values", (PyCFunction)tc_FDB_values, METH_NOARGS,
  //  NULL},
  {"iteritems", (PyCFunction)tc_FDB_GetIter_items, METH_NOARGS,
    NULL},
  {"iterkeys", (PyCFunction)tc_FDB_GetIter_keys, METH_NOARGS,
    NULL},
  {"itervalues", (PyCFunction)tc_FDB_GetIter_values, METH_NOARGS,
    NULL},
  // {"addint", (PyCFunction)tc_FDB_addint, METH_VARARGS | METH_KEYWORDS,
  //  "Add an integer to a record in a fixed-length database object."},
  // {"adddouble", (PyCFunction)tc_FDB_adddouble, METH_VARARGS | METH_KEYWORDS,
  //  "Add a real number to a record in a fixed-length database object."},
  {NULL, NULL, 0, NULL}
};

/* Hack to implement "key in dict" */
static PySequenceMethods tc_FDB_as_sequence = {
  0,                                            /* sq_length */
  0,                                            /* sq_concat */
  0,                                            /* sq_repeat */
  0,                                            /* sq_item */
  0,                                            /* sq_slice */
  0,                                            /* sq_ass_item */
  0,                                            /* sq_ass_slice */
  (objobjproc)tc_FDB_contains,                  /* sq_contains */
  0,                                            /* sq_inplace_concat */
  0,                                            /* sq_inplace_repeat */
};

static PyMappingMethods tc_FDB_as_mapping = {
  (lenfunc)tc_FDB_length,                       /* mp_length */
  (binaryfunc)tc_FDB___getitem__,               /* mp_subscript */
  (objobjargproc)tc_FDB___setitem__,            /* mp_ass_subscript */
};

PyTypeObject tc_FDBType = {
  #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
  #else
    PyVarObject_HEAD_INIT(NULL, 0)
  #endif
  "tc.FDB",                                     /* tp_name */
  sizeof(tc_FDB),                               /* tp_basicsize */
  0,                                            /* tp_itemsize */
  (destructor)tc_FDB_dealloc,                   /* tp_dealloc */
  0,                                            /* tp_print */
  0,                                            /* tp_getattr */
  0,                                            /* tp_setattr */
  0,                                            /* tp_compare */
  0,                                            /* tp_repr */
  0,                                            /* tp_as_number */
  &tc_FDB_as_sequence,                          /* tp_as_sequence */
  &tc_FDB_as_mapping,                           /* tp_as_mapping */
  tc_FDB_hash,                                  /* tp_hash  */
  0,                                            /* tp_call */
  0,                                            /* tp_str */
  0,                                            /* tp_getattro */
  0,                                            /* tp_setattro */
  0,                                            /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,     /* tp_flags */
  "Tokyo Cabinet fixed-length database",        /* tp_doc */
  0,                                            /* tp_traverse */
  0,                                            /* tp_clear */
  0,                                            /* tp_richcompare */
  0,                                            /* tp_weaklistoffset */
  (getiterfunc)tc_FDB_GetIter_keys,             /* tp_iter */
  (iternextfunc)tc_FDB_iternext,                /* tp_iternext */
  tc_FDB_methods,                               /* tp_methods */
  0,                                            /* tp_members */
  0,                                            /* tp_getset */
  0,                                            /* tp_base */
  0,                                            /* tp_dict */
  0,                                            /* tp_descr_get */
  0,                                            /* tp_descr_set */
  0,                                            /* tp_dictoffset */
  0,                                            /* tp_init */
  0,                                            /* tp_alloc */
  tc_FDB_new,                                   /* tp_new */
};

int tc_FDB_register(PyObject *module) {
  log_trace("ENTER");
  if (PyType_Ready(&tc_FDBType) == 0)
    return PyModule_AddObject(module, "FDB", (PyObject *)&tc_FDBType);
  return -1;
}