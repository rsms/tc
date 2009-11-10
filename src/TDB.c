#include "TDB.h"
#include "TDBQuery.h"
#include "util.h"

/* Private --------------------------------------------------------------- */

static void _set_tdb_error(TCTDB *db) {
  log_trace("ENTER");
  int ecode = tctdbecode(db);
  const char *msg = tctdberrmsg(ecode);
  if (ecode == TCENOREC) {
    PyErr_SetString(PyExc_KeyError, msg);
  } else {
    log_debug("TDB error: %s", msg);
    tc_Error_SetCodeAndString(ecode, msg);
  }
}


static bool _open(tc_TDB *self, PyObject *args, PyObject *keywds) {
  int omode = 0;
  char *path = NULL;
  static char *kwlist[] = {"path", "omode", NULL};
  
  if (PyArg_ParseTupleAndKeywords(args, keywds, "|si:open", kwlist, &path, &omode)) {
    if (path && omode) {
      bool result;
      Py_BEGIN_ALLOW_THREADS
      result = tctdbopen(self->db, path, omode);
      Py_END_ALLOW_THREADS
      if (!result) {
        _set_tdb_error(self->db);
        return false;
      }
    }
    return true;
  }
  
  return false; /* PyArg_ParseTupleAndKeywords failed */
}





/* Public ---------------------------------------------------------------- */

TC_XDB_OPEN(tc_TDB_open,tc_TDB,tc_TDB_new,tctdbopen,db,tc_TDB_dealloc,_set_tdb_error);

static void tc_TDB_dealloc(tc_TDB *self) {
  log_trace("ENTER");
  if (self->db) {
    Py_BEGIN_ALLOW_THREADS
    tctdbdel(self->db);
    Py_END_ALLOW_THREADS
  }
  PyObject_Del(self);
}


static PyObject *tc_TDB_new(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  tc_TDB *self;
  
  if (!(self = (tc_TDB *)type->tp_alloc(type, 0))) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc tc_TDB instance");
    return NULL;
  }
  
  self->db = NULL;
  
  if ( !(self->db = tctdbnew()) ) {
    tc_TDB_dealloc(self);
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc TCTDB struct");
    return NULL;
  }
  
  if (!_open(self, args, keywds)) {
    tc_TDB_dealloc(self);
    return NULL;
  }
  
  return (PyObject *)self;
}


static long tc_TDB_hash(PyObject *self) {
  log_trace("ENTER");
  PyErr_SetString(PyExc_TypeError, "TDB objects are unhashable");
  return -1L;
}

/* Getting/Setting */

static PyObject *tc_TDB_put(tc_TDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  TCMAP *cols = NULL;
  uint32_t cols_count;
  PyObject *columns_dict = NULL, *key, *value, *retv = NULL;
  PyObject *bkey = NULL, *bvalue = NULL;
  Py_ssize_t it_pos;
  const void *kbuf, *vbuf;
  const void *pkbuf;
  int ksiz, vsiz;
  Py_ssize_t pksiz;
  
  static char *kwlist[] = {"key", "columns", NULL};
  
  if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#O:put", kwlist, &pkbuf, &pksiz, &columns_dict)) {
    return NULL;
  }
  
  if(!PyDict_Check(columns_dict)) {
    return PyErr_Format(PyExc_TypeError, "first argument must be a dictionary");
  }
  
  cols_count = (uint32_t)PyDict_Size(columns_dict);
  cols = tcmapnew2(cols_count);
  
  /* Build columns TCMAP */
  if (cols_count > 0) {
    it_pos = 0;
    while (PyDict_Next(columns_dict, &it_pos, &key, &value)) {
      /* Key */
      if (PyBytes_Check(key)) {
        kbuf = PyBytes_AS_STRING(key);
        ksiz = PyBytes_GET_SIZE(key);
      }
      else if (PyUnicode_Check(key)) {
        if ( (bkey = PyUnicode_AsUTF8String(key)) == NULL ) {
          goto error;
        }
        kbuf = PyBytes_AS_STRING(bkey);
        ksiz = PyBytes_GET_SIZE(bkey);
      }
      else {
        PyErr_Format(PyExc_TypeError, "column keys must be bytes or strings");
        goto error;
      }
      
      /* Value */
      if (PyBytes_Check(value)) {
        vbuf = PyBytes_AS_STRING(value);
        vsiz = PyBytes_GET_SIZE(value);
      }
      else if (PyUnicode_Check(value)) {
        if ( (bvalue = PyUnicode_AsUTF8String(value)) == NULL ) {
          goto error;
        }
        vbuf = PyBytes_AS_STRING(bvalue);
        vsiz = PyBytes_GET_SIZE(bvalue);
      }
      else {
        PyErr_Format(PyExc_TypeError, "column values must be bytes or strings");
        goto error;
      }
      
      /* Put cell (implies memcpy, thus it's safe to decref k and v after this call) */
      tcmapput(cols, kbuf, ksiz, vbuf, vsiz);
      
      /* if not NULL, decref and set to NULL */
      Py_CLEAR(bkey);
      Py_CLEAR(bvalue);
    }
  }
  
  /* Put columns */
  if(!tctdbput(self->db, pkbuf, (int)pksiz, cols)){
    _set_tdb_error(self->db);
    goto error;
  }
  
  /* If we came this far, everything is OK so we set None as return value */
  retv = Py_None;
  Py_INCREF(retv);
  
error:
  if (cols) {
    tcmapdel(cols);
  }
  Py_XDECREF(bkey);
  Py_XDECREF(bvalue);
  return retv;
}

TC_XDB_SetItem(tc_TDB_SetItem,tc_TDB,tchdbput,db,_set_tdb_error);


// bool tctdbtune(TCTDB *tdb, int64_t bnum, int8_t apow, int8_t fpow, uint8_t opts);
static PyObject *tc_TDB_tune(tc_TDB *self, PyObject *args, PyObject *kwargs) {

  static char *kwlist[] = {"bnum", "apow", "fpow", "opts", NULL};
  bool result;
  short apow, fpow;
  unsigned char opts;
  PY_LONG_LONG bnum;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "LhhB:tune", kwlist, &bnum, &apow, &fpow, &opts)) {
    return NULL;
  }
  if (!(char_bounds(apow) && char_bounds(fpow))) {
     return NULL;
  }


  Py_BEGIN_ALLOW_THREADS
  result = tctdbtune(self->db, bnum, apow, fpow, opts);
  Py_END_ALLOW_THREADS

  if (!result){
      _set_tdb_error(self->db);
      return NULL;
  }

  Py_RETURN_NONE;

}

static PyObject *tc_TDB_get(tc_TDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  TCMAP *cols = NULL;
  PyObject *columns_dict = NULL, *key, *value, *retv = NULL;
  const void *kptr, *vptr;
  int ksiz, vsiz;
  const void *pkbuf;
  Py_ssize_t pksiz;
  
  static char *kwlist[] = {"key", NULL};
  
  if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#:get", kwlist, &pkbuf, &pksiz)) {
    return NULL;
  }
  
  /* Retrieve columns */
  if ( (cols = tctdbget(self->db, pkbuf, pksiz)) == NULL ) {
    _set_tdb_error(self->db);
    return NULL;
  }
  
  /* Alloc PyDict */
  if ( (columns_dict = PyDict_New()) == NULL ) {
    return NULL;
  }
  
  /* Transponse TCMAP to PyDict */
  tcmapiterinit(cols);
  while((kptr = tcmapiternext(cols, &ksiz)) != NULL){
    /* Key */
    if ( (key = PyBytes_FromStringAndSize((const char *)kptr, (Py_ssize_t)ksiz)) == NULL ) {
      goto error;
    }
    
    /* Value */
    if ( (vptr = tcmapget(cols, kptr, ksiz, &vsiz)) == NULL ) {
      _set_tdb_error(self->db);
      goto error;
    }
    if ( (value = PyBytes_FromStringAndSize((const char *)vptr, (Py_ssize_t)vsiz)) == NULL ) {
      Py_DECREF(key);
      goto error;
    }
    
    /* This function "steals" (or take over) the references, thus no decrefs here */
    if (PyDict_SetItem(columns_dict, key, value) != 0) {
      Py_DECREF(key);
      Py_DECREF(value);
      goto error;
    }
  }
  
  /* No incref here as we are giving away our reference, created by PyDict_New */
  retv = columns_dict;
  
error:
  if (cols) {
    tcmapdel(cols);
  }
  if (retv == NULL && columns_dict) {
    Py_DECREF(columns_dict);
  }
  return retv;
}


static PyObject *tc_TDB_delete(tc_TDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  const void *pkbuf;
  Py_ssize_t pksiz;
  
  static char *kwlist[] = {"key", NULL};
  
  if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#:delete", kwlist, &pkbuf, &pksiz)) {
    return NULL;
  }
  
  if ( ! tctdbout(self->db, pkbuf, pksiz) ) {
    _set_tdb_error(self->db);
    return NULL;
  }
  
  Py_RETURN_NONE;
}


static PyObject *tc_TDB_query(tc_TDB *self) {
  log_trace("ENTER");
  return (PyObject *)tc_TDBQuery_new_capi(self->db);
}


/* Type ------------------------------------------------------------------ */


/* methods of classes */
static PyMethodDef tc_TDB_methods[] = {
  {"open", (PyCFunction)tc_TDB_open, METH_VARARGS | METH_KEYWORDS,
    "Open a table."},
  {"put", (PyCFunction)tc_TDB_put, METH_VARARGS | METH_KEYWORDS,
    "Store a record."},
  {"get", (PyCFunction)tc_TDB_get, METH_VARARGS | METH_KEYWORDS,
    "Retrieve a record."},
  {"tune", (PyCFunction)tc_TDB_tune, METH_VARARGS | METH_KEYWORDS,
    "tune the database"},
  {"delete", (PyCFunction)tc_TDB_delete, METH_VARARGS | METH_KEYWORDS,
    "Remove a record."},
  {"out", (PyCFunction)tc_TDB_delete, METH_VARARGS | METH_KEYWORDS,
    "Alias of delete()."},
  {"query", (PyCFunction)tc_TDB_query, METH_NOARGS,
    "Query the table."},

  {NULL, NULL, 0, NULL}
};


PyTypeObject tc_TDBType = {
  #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(NULL)
    0,                  /*ob_size*/
  #else
    PyVarObject_HEAD_INIT(NULL, 0)
  #endif
  "tc.TDB",                                  /* tp_name */
  sizeof(tc_TDB),                             /* tp_basicsize */
  0,                                           /* tp_itemsize */
  (destructor)tc_TDB_dealloc,                 /* tp_dealloc */
  0,                                           /* tp_print */
  0,                                           /* tp_getattr */
  0,                                           /* tp_setattr */
  0,                                           /* tp_compare */
  0,                                           /* tp_repr */
  0,                                           /* tp_as_number */
  0,                                          /* tp_as_sequence */
  0,                                            /* tp_as_mapping */
  tc_TDB_hash,                                 /* tp_hash  */
  0,                                           /* tp_call */
  0,                                           /* tp_str */
  0,                                           /* tp_getattro */
  0,                                           /* tp_setattro */
  0,                                           /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
  "Tokyo Cabinet Table database",                /* tp_doc */
  0,                                           /* tp_traverse */
  0,                                           /* tp_clear */
  0,                                           /* tp_richcompare */
  0,                                           /* tp_weaklistoffset */
  (getiterfunc)0,                             /* tp_iter */
  (iternextfunc)0,                            /* tp_iternext */
  tc_TDB_methods,                             /* tp_methods */
  0,                                           /* tp_members */
  0,                                           /* tp_getset */
  0,                                           /* tp_base */
  0,                                           /* tp_dict */
  0,                                           /* tp_descr_get */
  0,                                           /* tp_descr_set */
  0,                                           /* tp_dictoffset */
  0,                                           /* tp_init */
  0,                                           /* tp_alloc */
  tc_TDB_new,                                 /* tp_new */
};

int tc_TDB_register(PyObject *module) {
  log_trace("ENTER");
  if (PyType_Ready(&tc_TDBType) == 0)
    return PyModule_AddObject(module, "TDB", (PyObject *)&tc_TDBType);
  return -1;
}
