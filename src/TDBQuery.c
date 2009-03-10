#include "TDBQuery.h"
#include "TDB.h"
#include "util.h"

/* Private --------------------------------------------------------------- */



/* Public ---------------------------------------------------------------- */


static void tc_TDBQuery_dealloc(tc_TDBQuery *self) {
  log_trace("ENTER");
  if (self->qry) {
    tctdbqrydel(self->qry);
  }
  PyObject_Del(self);
}


tc_TDBQuery *tc_TDBQuery_new_capi(TCTDB *tdb) {
  log_trace("ENTER");
  tc_TDBQuery *self;
  
  if (  ( self = (tc_TDBQuery *)tc_TDBQueryType.tp_alloc(&tc_TDBQueryType, 0) ) == NULL  ) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc tc_TDBQuery instance");
    return NULL;
  }
  
  self->qry = tctdbqrynew(tdb);
  
  return self;
}


static PyObject *tc_TDBQuery_new(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  tc_TDBQuery *self;
  PyObject *tdb;
  static char *kwlist[] = {"tdb", NULL};
  
  if (!(self = (tc_TDBQuery *)type->tp_alloc(type, 0))) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc tc_TDBQuery instance");
    return NULL;
  }
  
  self->qry = NULL;
  
  if (!PyArg_ParseTupleAndKeywords(args, keywds, "O:__new__", kwlist, &tdb)) {
    tc_TDBQuery_dealloc(self);
    return NULL;
  }
  
  if (!tc_TDB_Check(tdb)) {
    tc_TDBQuery_dealloc(self);
    PyErr_Format(PyExc_TypeError, "first argument must be a tc.TDB object");
    return NULL;
  }
  
  self->qry = tctdbqrynew( ((tc_TDB *)tdb)->db );
  
  return (PyObject *)self;
}


static long tc_TDBQuery_hash(PyObject *self) {
  log_trace("ENTER");
  PyErr_SetString(PyExc_TypeError, "TDBQuery objects are unhashable");
  return -1L;
}


/* Iteration */


static PyObject *tc_TDBQuery_keys(tc_TDBQuery *self) {
  log_trace("ENTER");
  TCLIST *res;
  PyObject *pylist, *key;
  const char *pkbuf;
  int pksiz, i;
  
  res = tctdbqrysearch(self->qry);
  
  if ( (pylist = PyList_New( (Py_ssize_t)TCLISTNUM(res) )) == NULL ) {
    tclistdel(res);
    return NULL;
  }
  
  for(i = 0; i < TCLISTNUM(res); i++){
    pkbuf = tclistval(res, i, &pksiz);
    if ( (key = PyBytes_FromStringAndSize(pkbuf, pksiz)) == NULL ) {
      Py_CLEAR(pylist);
      break;
    }
    PyList_SET_ITEM(pylist, i, key);
  }
  
  tclistdel(res);
  
  return pylist;
}


static PyObject *tc_TDBQuery_filter(tc_TDBQuery *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  const char *column;
  int operation = TDBQCSTREQ;
  const char *expression;
  static char *kwlist[] = {"column", "operation", "expression", NULL};
  
  if (!PyArg_ParseTupleAndKeywords(args, keywds, "ziz:filter", kwlist, &column, &operation, &expression)) {
    return NULL;
  }
  
  if (expression == NULL) {
    expression = "";
  }
  
  if (column == NULL) {
    column = ""; /* primary key */
  }
  
  tctdbqryaddcond(self->qry, column, operation, expression);
  
  Py_INCREF(self);
  return (PyObject *)self;
}


/* if type is negative, any order flag is cleared (results will not be ordered/are returned in natural order) */
static PyObject *tc_TDBQuery_order(tc_TDBQuery *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  const char *column;
  int type = TDBQOSTRASC;
  static char *kwlist[] = {"column", "type", NULL};
  
  if (!PyArg_ParseTupleAndKeywords(args, keywds, "z|i:order", kwlist, &column, &type)) {
    return NULL;
  }
  
  if (column == NULL) {
    column = ""; /* primary key */
  }
  
  if (type > -1) {
    tctdbqrysetorder(self->qry, column, type);
  }
  else {
    /* clear order flag */
    if(self->qry->oname) free(self->qry->oname);
    self->qry->oname = NULL;
    self->qry->otype = TDBQOSTRASC;
  }
  
  Py_INCREF(self);
  return (PyObject *)self;
}


/* Type ------------------------------------------------------------------ */


/* methods of classes */
static PyMethodDef tc_TDBQuery_methods[] = {
  {"keys", (PyCFunction)tc_TDBQuery_keys, METH_NOARGS,
    "Retrieve primary keys."},
  {"filter", (PyCFunction)tc_TDBQuery_filter, METH_VARARGS | METH_KEYWORDS,
    "Filter by condition."},
  {"order", (PyCFunction)tc_TDBQuery_order, METH_VARARGS | METH_KEYWORDS,
    "Set order."},
  {NULL, NULL, 0, NULL}
};


PyTypeObject tc_TDBQueryType = {
  #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(NULL)
    0,                  /*ob_size*/
  #else
    PyVarObject_HEAD_INIT(NULL, 0)
  #endif
  "tc.TDBQuery",                                  /* tp_name */
  sizeof(tc_TDBQuery),                             /* tp_basicsize */
  0,                                           /* tp_itemsize */
  (destructor)tc_TDBQuery_dealloc,                 /* tp_dealloc */
  0,                                           /* tp_print */
  0,                                           /* tp_getattr */
  0,                                           /* tp_setattr */
  0,                                           /* tp_compare */
  0,                                           /* tp_repr */
  0,                                           /* tp_as_number */
  0,                                          /* tp_as_sequence */
  0,                                            /* tp_as_mapping */
  tc_TDBQuery_hash,                                 /* tp_hash  */
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
  (getiterfunc)0,                              /* tp_iter */
  (iternextfunc)0,                             /* tp_iternext */
  tc_TDBQuery_methods,                         /* tp_methods */
  0,                                           /* tp_members */
  0,                                           /* tp_getset */
  0,                                           /* tp_base */
  0,                                           /* tp_dict */
  0,                                           /* tp_descr_get */
  0,                                           /* tp_descr_set */
  0,                                           /* tp_dictoffset */
  0,                                           /* tp_init */
  0,                                           /* tp_alloc */
  tc_TDBQuery_new,                                 /* tp_new */
};

int tc_TDBQuery_register(PyObject *module) {
  log_trace("ENTER");
  if (PyType_Ready(&tc_TDBQueryType) == 0)
    return PyModule_AddObject(module, "TDBQuery", (PyObject *)&tc_TDBQueryType);
  return -1;
}
