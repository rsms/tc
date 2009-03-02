#include "HDB.h"
#include "util.h"

/* Private --------------------------------------------------------------- */

static void tc_Error_SetHDB(TCHDB *hdb) {
  log_trace("ENTER");
  int ecode = tchdbecode(hdb);
  if (ecode == TCENOREC) {
    PyErr_SetString(PyExc_KeyError, tchdberrmsg(ecode));
  } else {
    tc_Error_SetCodeAndString(ecode, tchdberrmsg(ecode));
  }
}

#define tc_HDB_TUNE_OR_OPT(a,b,c) \
  static PyObject * \
  a(tc_HDB *self, PyObject *args, PyObject *keywds) { \
    log_trace("ENTER");\
    bool result; \
    short apow, fpow; \
    PY_LONG_LONG bnum; \
    unsigned char opts; \
    static char *kwlist[] = {"bnum", "apow", "fpow", "opts", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "LhhB:" #b, kwlist, \
                                     &bnum, &apow, &fpow, &opts)) { \
      return NULL; \
    } \
    if (!(char_bounds(apow) && char_bounds(fpow))) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    result = c(self->hdb, bnum, apow, fpow, opts); \
    Py_END_ALLOW_THREADS \
  \
    if (!result) { \
      tc_Error_SetHDB(self->hdb); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }

/* Public --------------------------------------------------------------- */

static long tc_HDB_Hash(PyObject *self) {
  log_trace("ENTER");
  PyErr_SetString(PyExc_TypeError, "HDB objects are unhashable");
  return -1L;
}

static PyObject *tc_HDB_errmsg(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  int ecode;
  static char *kwlist[] = {"ecode", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "i:errmsg", kwlist,
                                   &ecode)) {
    return NULL;
  }
  return PyBytes_FromString(tchdberrmsg(ecode));
}

static void tc_HDB_dealloc(tc_HDB *self) {
  log_trace("ENTER");
  /* NOTE: tchdbsel closes hdb implicitly */
  /*
  if (self->hdb->flags & HDBFOPEN) {
    Py_BEGIN_ALLOW_THREADS
    tchdbclose(self->hdb);
    Py_END_ALLOW_THREADS
  }
  */
  if (self->hdb) {
    Py_BEGIN_ALLOW_THREADS
    tchdbdel(self->hdb);
    Py_END_ALLOW_THREADS
  }
  PyObject_Del(self);
}

static PyObject *tc_HDB_new(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  tc_HDB *self;
  if (!(self = (tc_HDB *)type->tp_alloc(type, 0))) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc tc_HDB instance");
    return NULL;
  }
  if ((self->hdb = tchdbnew())) {
    int omode = 0;
    char *path = NULL;
    static char *kwlist[] = {"path", "omode", NULL};

    if (PyArg_ParseTupleAndKeywords(args, keywds, "|si:open", kwlist,
                                    &path, &omode)) {
      if (path && omode) {
        bool result;
        Py_BEGIN_ALLOW_THREADS
        result = tchdbopen(self->hdb, path, omode);
        Py_END_ALLOW_THREADS
        if (result) {
          return (PyObject *)self;
        }
      } else {
        return (PyObject *)self;
      }
      tc_Error_SetHDB(self->hdb);
    }
  } else {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc TCHDB instance");
  }
  tc_HDB_dealloc(self);
  return NULL;
}

static PyObject *tc_HDB_ecode(tc_HDB *self) {
  log_trace("ENTER");
  return NUMBER_FromLong((long)tchdbecode(self->hdb));
}

TC_BOOL_NOARGS(tc_HDB_setmutex,tc_HDB,tchdbsetmutex,hdb,tc_Error_SetHDB,hdb);
tc_HDB_TUNE_OR_OPT(tc_HDB_tune, tune, tchdbtune);
TC_XDB_OPEN(tc_HDB_open,tc_HDB,tc_HDB_new,tchdbopen,hdb,tc_HDB_dealloc,tc_Error_SetHDB);
TC_BOOL_NOARGS(tc_HDB_close,tc_HDB,tchdbclose,hdb,tc_Error_SetHDB,hdb);
TC_XDB_PUT(tc_HDB_put, tc_HDB, put, tchdbput, hdb, tc_Error_SetHDB);
TC_XDB_PUT(tc_HDB_putkeep, tc_HDB, putkeep, tchdbputkeep, hdb, tc_Error_SetHDB);
TC_XDB_PUT(tc_HDB_putcat, tc_HDB, putcat, tchdbputcat, hdb, tc_Error_SetHDB);
TC_XDB_PUT(tc_HDB_putasync, tc_HDB, putasync, tchdbputasync, hdb, tc_Error_SetHDB);
TC_BOOL_KEYARGS(tc_HDB_out,tc_HDB,out,tchdbout,hdb,tc_Error_SetHDB,hdb);
TC_STRINGL_KEYARGS(tc_HDB_get,tc_HDB,get,tchdbget,hdb,tc_Error_SetHDB);
TC_INT_KEYARGS(tc_HDB_vsiz,tc_HDB,vsiz,tchdbvsiz,hdb,tc_Error_SetHDB);

TC_BOOL_NOARGS(tc_HDB_iterinit,tc_HDB,tchdbiterinit,hdb,tc_Error_SetHDB,hdb);

static PyObject *tc_HDB_GetIter(tc_HDB *self, tc_itertype_t itype) {
  log_trace("ENTER");
  if (tc_HDB_iterinit(self)) {
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

TC_XDB_iters(tc_HDB,tc_HDB_GetIter_keys,tc_HDB_GetIter_values,tc_HDB_GetIter_items,tc_HDB_GetIter);

static PyObject *tc_HDB_iternext(tc_HDB *self) {
  log_trace("ENTER");
  if (self->itype == tc_iter_key_t) {
    void *key;
    int key_len;

    Py_BEGIN_ALLOW_THREADS
    key = tchdbiternext(self->hdb, &key_len);
    Py_END_ALLOW_THREADS

    if (key) {
      PyObject *_key;
      _key = PyBytes_FromStringAndSize(key, key_len);
      free(key);
      return _key;
    }
    return NULL;
  } else {
    TC_GET_TCXSTR_KEY_VALUE(tchdbiternext3,self->hdb)
    if (result) {
      if (self->itype == tc_iter_value_t) {
        ret = PyBytes_FromStringAndSize(tcxstrptr(value), tcxstrsize(value));
      } else {
        ret = Py_BuildValue("(s#s#)", tcxstrptr(key), tcxstrsize(key),
                                      tcxstrptr(value), tcxstrsize(value));
      }
    }
    TC_CLEAR_TCXSTR_KEY_VALUE()
  }
}

TC_BOOL_NOARGS(tc_HDB_sync,tc_HDB,tchdbsync,hdb,tc_Error_SetHDB,hdb);
tc_HDB_TUNE_OR_OPT(tc_HDB_optimize, optimize, tchdboptimize);
TC_STRING_NOARGS(tc_HDB_path,tc_HDB,tchdbpath,hdb,tc_Error_SetHDB);
TC_U_LONG_LONG_NOARGS(tc_HDB_rnum, tc_HDB, tchdbrnum, hdb, tchdbecode, tc_Error_SetHDB);
TC_U_LONG_LONG_NOARGS(tc_HDB_fsiz, tc_HDB, tchdbrnum, hdb, tchdbecode, tc_Error_SetHDB);
TC_BOOL_NOARGS(tc_HDB_vanish,tc_HDB,tchdbvanish,hdb,tc_Error_SetHDB,hdb);
TC_BOOL_PATHARGS(tc_HDB_copy, tc_HDB, copy, tchdbcopy, hdb, tc_Error_SetHDB);
/* todo: features for experts */
TC_XDB_Contains(tc_HDB_Contains,tc_HDB,tchdbvsiz,hdb);
TC_XDB___contains__(tc_HDB___contains__,tc_HDB,tc_HDB_Contains);
TC_XDB___getitem__(tc_HDB___getitem__,tc_HDB,tchdbget,hdb,tc_Error_SetHDB);
TC_XDB_rnum(TCHDB_rnum,TCHDB,tchdbrnum);

static PyObject *tc_HDB_keys(tc_HDB *self) {
  log_trace("ENTER");
  int i;
  PyObject *ret;
  if (!tc_HDB_iterinit((tc_HDB *)self) ||
      !(ret = PyList_New(TCHDB_rnum(self->hdb)))) {
    return NULL;
  }
  for (i = 0; ; i++) {
    char *key;
    int key_len;
    PyObject *_key;

    Py_BEGIN_ALLOW_THREADS
    key = tchdbiternext(self->hdb, &key_len);
    Py_END_ALLOW_THREADS
    if (!key) { break; }
    _key = PyBytes_FromStringAndSize(key, key_len);
    free(key);
    if (!_key) {
      Py_DECREF(ret);
      return NULL;
    }
    PyList_SET_ITEM(ret, i, _key);
  }
  return ret;
}

static PyObject *tc_HDB_items(tc_HDB *self) {
  log_trace("ENTER");
  int i, n = TCHDB_rnum(self->hdb);
  PyObject *ret, *item;
  if (!tc_HDB_iterinit((tc_HDB *)self) ||
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
    key = tchdbiternext(self->hdb, &key_len);
    Py_END_ALLOW_THREADS
    if (!key) { break; }

    Py_BEGIN_ALLOW_THREADS
    value = tchdbget(self->hdb, key, key_len, &value_len);
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

static PyObject *tc_HDB_values(tc_HDB *self) {
  log_trace("ENTER");
  int i;
  PyObject *ret;
  if (!tc_HDB_iterinit((tc_HDB *)self) ||
      !(ret = PyList_New(TCHDB_rnum(self->hdb)))) {
    return NULL;
  }
  for (i = 0; ; i++) {
    char *key, *value;
    int key_len, value_len;

    Py_BEGIN_ALLOW_THREADS
    key = tchdbiternext(self->hdb, &key_len);
    Py_END_ALLOW_THREADS

    if (!key) { break; }

    Py_BEGIN_ALLOW_THREADS
    value = tchdbget(self->hdb, key, key_len, &value_len);
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

TC_XDB_length(tc_HDB_length,tc_HDB,TCHDB_rnum,hdb);
TC_XDB_subscript(tc_HDB_subscript,tc_HDB,tchdbget,hdb,tc_Error_SetHDB);
TC_XDB_DelItem(tc_HDB_DelItem,tc_HDB,tchdbout,hdb,tc_Error_SetHDB);
TC_XDB_SetItem(tc_HDB_SetItem,tc_HDB,tchdbput,hdb,tc_Error_SetHDB);
TC_XDB_ass_sub(tc_HDB_ass_sub,tc_HDB,tc_HDB_SetItem,tc_HDB_DelItem);

TC_XDB_addint(tc_HDB_addint,tc_HDB,addint,tchdbaddint,hdb,tc_Error_SetHDB);
TC_XDB_adddouble(tc_HDB_adddouble,tc_HDB,addint,tchdbadddouble,hdb,tc_Error_SetHDB);

/* methods of classes */
static PyMethodDef tc_HDB_methods[] = {
  {"errmsg", (PyCFunction)tc_HDB_errmsg, METH_VARARGS | METH_KEYWORDS | METH_CLASS,
    "Get the message string corresponding to an error code."},
  {"ecode", (PyCFunction)tc_HDB_ecode, METH_NOARGS,
    "Get the last happened error code of a hash database object."},
  {"setmutex", (PyCFunction)tc_HDB_setmutex, METH_NOARGS,
    "Set mutual exclusion control of a hash database object for threading."},
  {"tune", (PyCFunction)tc_HDB_tune, METH_VARARGS | METH_KEYWORDS,
    "Set the tuning parameters of a hash database object."},
  {"open", (PyCFunction)tc_HDB_open, METH_VARARGS | METH_KEYWORDS,
    "Open a database file and connect a hash database object."},
  {"close", (PyCFunction)tc_HDB_close, METH_NOARGS,
    "Close a hash database object."},
  {"put", (PyCFunction)tc_HDB_put, METH_VARARGS | METH_KEYWORDS,
    "Store a record into a hash database object."},
  {"putkeep", (PyCFunction)tc_HDB_putkeep, METH_VARARGS | METH_KEYWORDS,
    "Store a new record into a hash database object."},
  {"putcat", (PyCFunction)tc_HDB_putcat, METH_VARARGS | METH_KEYWORDS,
    "Concatenate a value at the end of the existing record in a hash database object."},
  {"putasync", (PyCFunction)tc_HDB_putasync, METH_VARARGS | METH_KEYWORDS,
    "Store a record into a hash database object in asynchronous fashion."},
  {"out", (PyCFunction)tc_HDB_out, METH_VARARGS | METH_KEYWORDS,
    "Remove a record of a hash database object."},
  {"get", (PyCFunction)tc_HDB_get, METH_VARARGS | METH_KEYWORDS,
    "Retrieve a record in a hash database object."},
  {"vsiz", (PyCFunction)tc_HDB_vsiz, METH_VARARGS | METH_KEYWORDS,
    "Get the size of the value of a record in a hash database object."},
  {"iterinit", (PyCFunction)tc_HDB_iterinit, METH_NOARGS,
    "Initialize the iterator of a hash database object."},
  {"iternext", (PyCFunction)tc_HDB_iternext, METH_NOARGS,
    "Get the next extensible objects of the iterator of a hash database object."},
  {"sync", (PyCFunction)tc_HDB_sync, METH_NOARGS,
    "Synchronize updated contents of a hash database object with the file and the device."},
  {"optimize", (PyCFunction)tc_HDB_optimize, METH_VARARGS | METH_KEYWORDS,
    "Optimize the file of a hash database object."},
  {"vanish", (PyCFunction)tc_HDB_vanish, METH_NOARGS,
    "Remove all records of a hash database object."},
  {"path", (PyCFunction)tc_HDB_path, METH_NOARGS,
    "Get the file path of a hash database object."},
  {"copy", (PyCFunction)tc_HDB_copy, METH_VARARGS | METH_KEYWORDS,
    "Copy the database file of a hash database object."},
  {"rnum", (PyCFunction)tc_HDB_rnum, METH_NOARGS,
    "Get the number of records of a hash database object."},
  {"fsiz", (PyCFunction)tc_HDB_fsiz, METH_NOARGS,
   "Get the size of the database file of a hash database object."},
  {"__contains__", (PyCFunction)tc_HDB___contains__, METH_O | METH_COEXIST,
    NULL},
  {"__getitem__", (PyCFunction)tc_HDB___getitem__, METH_O | METH_COEXIST,
    NULL},
  {"has_key", (PyCFunction)tc_HDB___contains__, METH_O,
    NULL},
  {"keys", (PyCFunction)tc_HDB_keys, METH_NOARGS,
    NULL},
  {"items", (PyCFunction)tc_HDB_items, METH_NOARGS,
    NULL},
  {"values", (PyCFunction)tc_HDB_values, METH_NOARGS,
    NULL},
  {"iteritems", (PyCFunction)tc_HDB_GetIter_items, METH_NOARGS,
    NULL},
  {"iterkeys", (PyCFunction)tc_HDB_GetIter_keys, METH_NOARGS,
    NULL},
  {"itervalues", (PyCFunction)tc_HDB_GetIter_values, METH_NOARGS,
    NULL},
  {"addint", (PyCFunction)tc_HDB_addint, METH_VARARGS | METH_KEYWORDS,
    "Add an integer to a record in a hash database object."},
  {"adddouble", (PyCFunction)tc_HDB_adddouble, METH_VARARGS | METH_KEYWORDS,
    "Add a real number to a record in a hash database object."},
  {NULL, NULL, 0, NULL}
};


/* Hack to implement "key in dict" */
static PySequenceMethods tc_HDB_as_sequence = {
  0,                             /* sq_length */
  0,                             /* sq_concat */
  0,                             /* sq_repeat */
  0,                             /* sq_item */
  0,                             /* sq_slice */
  0,                             /* sq_ass_item */
  0,                             /* sq_ass_slice */
  (objobjproc)tc_HDB_Contains,  /* sq_contains */
  0,                             /* sq_inplace_concat */
  0,                             /* sq_inplace_repeat */
};

static PyMappingMethods tc_HDB_as_mapping = {
  (lenfunc)tc_HDB_length, /* mp_length (inquiry/lenfunc )*/
  (binaryfunc)tc_HDB_subscript, /* mp_subscript */
  (objobjargproc)tc_HDB_ass_sub, /* mp_ass_subscript */
};

PyTypeObject tc_HDBType = {
  #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(NULL)
    0,                  /*ob_size*/
  #else
    PyVarObject_HEAD_INIT(NULL, 0)
  #endif
  "tc.HDB",                                  /* tp_name */
  sizeof(tc_HDB),                             /* tp_basicsize */
  0,                                           /* tp_itemsize */
  (destructor)tc_HDB_dealloc,                 /* tp_dealloc */
  0,                                           /* tp_print */
  0,                                           /* tp_getattr */
  0,                                           /* tp_setattr */
  0,                                           /* tp_compare */
  0,                                           /* tp_repr */
  0,                                           /* tp_as_number */
  &tc_HDB_as_sequence,                        /* tp_as_sequence */
  &tc_HDB_as_mapping,                         /* tp_as_mapping */
  tc_HDB_Hash,                                /* tp_hash  */
  0,                                           /* tp_call */
  0,                                           /* tp_str */
  0,                                           /* tp_getattro */
  0,                                           /* tp_setattro */
  0,                                           /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
  "Tokyo Cabinet Hash database",                /* tp_doc */
  0,                                           /* tp_traverse */
  0,                                           /* tp_clear */
  0,                                           /* tp_richcompare */
  0,                                           /* tp_weaklistoffset */
  (getiterfunc)tc_HDB_GetIter_keys,           /* tp_iter */
  (iternextfunc)tc_HDB_iternext,              /* tp_iternext */
  tc_HDB_methods,                             /* tp_methods */
  0,                                           /* tp_members */
  0,                                           /* tp_getset */
  0,                                           /* tp_base */
  0,                                           /* tp_dict */
  0,                                           /* tp_descr_get */
  0,                                           /* tp_descr_set */
  0,                                           /* tp_dictoffset */
  0,                                           /* tp_init */
  0,                                           /* tp_alloc */
  tc_HDB_new,                                 /* tp_new */
};

int tc_HDB_register(PyObject *module) {
  log_trace("ENTER");
  if (PyType_Ready(&tc_HDBType) == 0)
    return PyModule_AddObject(module, "HDB", (PyObject *)&tc_HDBType);
  return -1;
}
