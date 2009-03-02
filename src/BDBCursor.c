#include "BDBCursor.h"
#include "util.h"

/* Private --------------------------------------------------------------- */

/* Public --------------------------------------------------------------- */

PyObject *tc_BDBCursor_new(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  tc_BDB *bdb;
  tc_BDBCursor *self;
  static char *kwlist[] = {"bdb", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!:new", kwlist,
                                   &tc_BDBType, &bdb)) {
    return NULL;
  }
  if (!(self = (tc_BDBCursor *)type->tp_alloc(type, 0))) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc tc_BDBCursor instance");
    return NULL;
  }

  Py_BEGIN_ALLOW_THREADS
  self->cur = tcbdbcurnew(bdb->bdb);
  Py_END_ALLOW_THREADS

  if (!self->cur) {
    Py_DECREF(self);
    tc_Error_SetBDB(bdb->bdb);
    return NULL;
  }
  Py_INCREF(bdb);
  self->bdb = bdb;
  return (PyObject *)self;
}

void tc_BDBCursor_dealloc(tc_BDBCursor *self) {
  log_trace("ENTER");
  Py_BEGIN_ALLOW_THREADS
  tcbdbcurdel(self->cur);
  Py_END_ALLOW_THREADS
  Py_XDECREF(self->bdb);
  PyObject_Del(self);
}

PyObject *tc_BDBCursor_first(tc_BDBCursor *self) {
  log_trace("ENTER");
  bool result;
  Py_BEGIN_ALLOW_THREADS
  result = tcbdbcurfirst(self->cur);
  Py_END_ALLOW_THREADS
  if (!result) {
    tc_Error_SetBDB(self->bdb->bdb);
    return NULL;
  }
  Py_RETURN_NONE;
}

TC_BOOL_NOARGS(tc_BDBCursor_last,tc_BDBCursor,tcbdbcurlast,cur,tc_Error_SetBDB,bdb->bdb);
TC_BOOL_KEYARGS(tc_BDBCursor_jump,tc_BDBCursor,jump,tcbdbcurjump,cur,tc_Error_SetBDB,bdb->bdb);
TC_BOOL_NOARGS(tc_BDBCursor_prev,tc_BDBCursor,tcbdbcurprev,cur,tc_Error_SetBDB,bdb->bdb);
TC_BOOL_NOARGS(tc_BDBCursor_next,tc_BDBCursor,tcbdbcurnext,cur,tc_Error_SetBDB,bdb->bdb);

static PyObject *tc_BDBCursor_put(tc_BDBCursor *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  bool result;
  char *value;
  int value_len, cpmode;
  static char *kwlist[] = {"value", "cpmode", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#i:put", kwlist,
                                   &value, &value_len, &cpmode)) {
    return NULL;
  }
  Py_BEGIN_ALLOW_THREADS
  result = tcbdbcurput(self->cur, value, value_len, cpmode);
  Py_END_ALLOW_THREADS

  if (!result) {
    tc_Error_SetBDB(self->bdb->bdb);
    return NULL;
  }
  Py_RETURN_NONE;
}

TC_BOOL_NOARGS(tc_BDBCursor_out,tc_BDBCursor,tcbdbcurout,cur,tc_Error_SetBDB,bdb->bdb);
TC_STRINGL_NOARGS(tc_BDBCursor_key,tc_BDBCursor,tcbdbcurkey,cur,tc_Error_SetBDB,bdb->bdb);
TC_STRINGL_NOARGS(tc_BDBCursor_val,tc_BDBCursor,tcbdbcurval,cur,tc_Error_SetBDB,bdb->bdb);

static PyObject *tc_BDBCursor_rec(tc_BDBCursor *self) {
  log_trace("ENTER");
  TC_GET_TCXSTR_KEY_VALUE(tcbdbcurrec,self->cur)
  if (result) {
    ret = Py_BuildValue("(s#s#)", tcxstrptr(key), tcxstrsize(key),
                                  tcxstrptr(value), tcxstrsize(value));
  }
  if (!ret) {
    tc_Error_SetBDB(self->bdb->bdb);
  }
  TC_CLEAR_TCXSTR_KEY_VALUE()
}

static PyObject *tc_BDBCursor_iternext(tc_BDBCursor *self) {
  log_trace("ENTER");
  /* TODO: performance up */
  TC_GET_TCXSTR_KEY_VALUE(tcbdbcurrec,self->cur)
  if (result) {
    switch (self->itype) {
      case tc_iter_key_t:
        ret = PyBytes_FromStringAndSize(tcxstrptr(key), tcxstrsize(key));
        break;
      case tc_iter_value_t:
        ret = PyBytes_FromStringAndSize(tcxstrptr(value), tcxstrsize(value));
        break;
      case tc_iter_item_t:
        ret = Py_BuildValue("(s#s#)", tcxstrptr(key), tcxstrsize(key),
                                      tcxstrptr(value), tcxstrsize(value));
        break;
    }
  }
  Py_BEGIN_ALLOW_THREADS
  result = tcbdbcurnext(self->cur);
  Py_END_ALLOW_THREADS
  TC_CLEAR_TCXSTR_KEY_VALUE()
}

static PyMethodDef tc_BDBCursor_methods[] = {
  {"first", (PyCFunction)tc_BDBCursor_first, METH_NOARGS,
    "Move a cursor object to the first record."},
  {"last", (PyCFunction)tc_BDBCursor_last, METH_NOARGS,
    "Move a cursor object to the last record."},
  {"jump", (PyCFunction)tc_BDBCursor_jump, METH_VARARGS | METH_KEYWORDS,
    "Move a cursor object to the front of records corresponding a key."},
  {"prev", (PyCFunction)tc_BDBCursor_prev, METH_NOARGS,
    "Move a cursor object to the previous record."},
  {"next", (PyCFunction)tc_BDBCursor_next, METH_NOARGS,
    "Move a cursor object to the next record."},
  {"put", (PyCFunction)tc_BDBCursor_put, METH_VARARGS | METH_KEYWORDS,
    "Insert a record around a cursor object."},
  {"out", (PyCFunction)tc_BDBCursor_out, METH_NOARGS,
    "Delete the record where a cursor object is."},
  {"key", (PyCFunction)tc_BDBCursor_key, METH_NOARGS,
    "Get the key of the record where the cursor object is."},
  {"val", (PyCFunction)tc_BDBCursor_val, METH_NOARGS,
    "Get the value of the record where the cursor object is."},
  {"rec", (PyCFunction)tc_BDBCursor_rec, METH_NOARGS,
    "Get the key and the value of the record where the cursor object is."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject tc_BDBCursorType = {
  #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(NULL)
    0,                  /*ob_size*/
  #else
    PyVarObject_HEAD_INIT(NULL, 0)
  #endif
  "tc.BDBCursor",                           /* tp_name */
  sizeof(tc_BDBCursor),                     /* tp_basicsize */
  0,                                        /* tp_itemsize */
  (destructor)tc_BDBCursor_dealloc,         /* tp_dealloc */
  0,                                        /* tp_print */
  0,                                        /* tp_getattr */
  0,                                        /* tp_setattr */
  0,                                        /* tp_compare */
  0,                                        /* tp_repr */
  0,                                        /* tp_as_number */
  0,                                        /* tp_as_sequence */
  0,                                        /* tp_as_mapping */
  0,                                        /* tp_hash  */
  0,                                        /* tp_call */
  0,                                        /* tp_str */
  0,                                        /* tp_getattro */
  0,                                        /* tp_setattro */
  0,                                        /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
  "Tokyo Cabinet B+ tree database cursor",  /* tp_doc */
  0,                                        /* tp_traverse */
  0,                                        /* tp_clear */
  0,                                        /* tp_richcompare */
  0,                                        /* tp_weaklistoffset */
  PyObject_SelfIter,                        /* tp_iter */
  (iternextfunc)tc_BDBCursor_iternext,      /* tp_iternext */
  tc_BDBCursor_methods,                     /* tp_methods */
  0,                                        /* tp_members */
  0,                                        /* tp_getset */
  0,                                        /* tp_base */
  0,                                        /* tp_dict */
  0,                                        /* tp_descr_get */
  0,                                        /* tp_descr_set */
  0,                                        /* tp_dictoffset */
  0,                                        /* tp_init */
  0,                                        /* tp_alloc */
  tc_BDBCursor_new,                         /* tp_new */
};


int tc_BDBCursor_register(PyObject *module) {
  log_trace("ENTER");
  if (PyType_Ready(&tc_BDBCursorType) == 0)
    return PyModule_AddObject(module, "BDBCursor", (PyObject *)&tc_BDBCursorType);
  return -1;
}
