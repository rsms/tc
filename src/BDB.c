#include "BDB.h"
#include "BDBCursor.h"
#include "util.h"

/* Private --------------------------------------------------------------- */

#define tc_BDB_TUNE_OR_OPT(a,b,c) \
  static PyObject * \
  a(tc_BDB *self, PyObject *args, PyObject *keywds) { \
    log_trace("ENTER"); \
    bool result; \
    short apow, fpow; \
    int lmemb, nmemb; \
    PY_LONG_LONG bnum; \
    unsigned char opts; \
    static char *kwlist[] = {"lmemb", "nmemb", "bnum", "apow", "fpow", "opts", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "iiLhhB:" #b, kwlist, \
                                     &lmemb, &nmemb, &bnum, \
                                     &apow, &fpow, &opts)) { \
      return NULL; \
    } \
    if (!(char_bounds(apow) && char_bounds(fpow))) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    result = c(self->bdb, lmemb, nmemb, bnum, apow, fpow, opts); \
    Py_END_ALLOW_THREADS \
  \
    if (!result) { \
      tc_Error_SetBDB(self->bdb); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }


#define TCLIST2PyList() \
  if (!list) { \
    tc_Error_SetBDB(self->bdb); \
    return NULL; \
  } else { \
    PyObject *ret; \
    int i, n = tclistnum(list); \
    if ((ret = PyList_New(n))) { \
      for (i = 0; i < n; i++) { \
        int value_len; \
        PyObject *_value; \
        const char *value; \
        value = tclistval(list, i, &value_len); \
        _value = PyBytes_FromStringAndSize(value, value_len); \
        PyList_SET_ITEM(ret, i, _value); \
      } \
    } \
    tclistdel(list); \
    return ret; \
  }


/* Public --------------------------------------------------------------- */

void tc_Error_SetBDB(TCBDB *bdb) {
  log_trace("ENTER");
  int ecode = tcbdbecode(bdb);
  if (ecode == TCENOREC) {
    PyErr_SetString(PyExc_KeyError, tcbdberrmsg(ecode));
  } else {
    tc_Error_SetCodeAndString(ecode, tcbdberrmsg(ecode));
  }
}

static long tc_BDB_Hash(PyObject *self) {
  log_trace("ENTER");
  PyErr_SetString(PyExc_TypeError, "BDB objects are unhashable");
  return -1;
}

static PyObject *tc_BDB_errmsg(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  int ecode;
  static char *kwlist[] = {"ecode", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "i:errmsg", kwlist,
                                   &ecode)) {
    return NULL;
  }
  return PyBytes_FromString(tcbdberrmsg(ecode));
}

static void tc_BDB_dealloc(tc_BDB *self) {
  log_trace("ENTER");
  /* NOTE: tcbdbsel closes bdb implicitly */
  /*
  if (self->bdb->flags & BDBFOPEN) {
    Py_BEGIN_ALLOW_THREADS
    tcbdbclose(self->bdb);
    Py_END_ALLOW_THREADS
  }
  */
  Py_XDECREF(self->cmp);
  Py_XDECREF(self->cmpop);
  if (self->bdb) {
    Py_BEGIN_ALLOW_THREADS
    tcbdbdel(self->bdb);
    Py_END_ALLOW_THREADS
  }
  PyObject_Del(self);
}

static PyObject *tc_BDB_new(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  tc_BDB *self;
  if (!(self = (tc_BDB *)type->tp_alloc(type, 0))) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc tc_BDB instance");
    return NULL;
  }
  /* NOTE: initialize member implicitly */
  self->cmp = self->cmpop = NULL;
  if ((self->bdb = tcbdbnew())) {
    int omode = 0;
    char *path = NULL;
    static char *kwlist[] = {"path", "omode", NULL};

    if (PyArg_ParseTupleAndKeywords(args, keywds, "|si:open", kwlist,
                                    &path, &omode)) {
      if (path && omode) {
        bool result;
        Py_BEGIN_ALLOW_THREADS
        result = tcbdbopen(self->bdb, path, omode);
        Py_END_ALLOW_THREADS
        if (result) {
          return (PyObject *)self;
        }
      } else {
        return (PyObject *)self;
      }
      tc_Error_SetBDB(self->bdb);
    }
  } else {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc TCBDB instance");
  }
  tc_BDB_dealloc(self);
  return NULL;
}

static PyObject *tc_BDB_ecode(tc_BDB *self) {
  log_trace("ENTER");
  return NUMBER_FromLong((long)tcbdbecode(self->bdb));
}

TC_BOOL_NOARGS(tc_BDB_setmutex,tc_BDB,tcbdbsetmutex,bdb,tc_Error_SetBDB,bdb);

static int TCBDB_cmpfunc(const char *aptr, int asiz,
              const char *bptr, int bsiz, tc_BDB *self)
{
  log_trace("ENTER");
  int ret = 0;
  PyObject *args, *result;
  PyGILState_STATE gstate;

  if ((args = Py_BuildValue("(s#s#O)", aptr, asiz, bptr, bsiz, self->cmpop))) {
    /* TODO: set error */
    return 0;
  }
  gstate = PyGILState_Ensure();
  result = PyEval_CallObject(self->cmp, args);
  Py_DECREF(args);
  if (!result) {
    /* TODO: set error */
    goto exit;
  }
  ret = PyLong_AsLong(result);
  Py_DECREF(result);
exit:
  PyGILState_Release(gstate);
  return ret;
}

static PyObject *tc_BDB_setcmpfunc(tc_BDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  bool result;
  PyObject *cmp, *cmpop = NULL;
  static char *kwlist[] = {"cmp", "cmpop", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "O|O:setcmpfunc", kwlist,
                                   &cmp, &cmpop)
      || !PyCallable_Check(cmp)) {
    return NULL;
  }
  if (!cmpop) {
    Py_INCREF(Py_None);
    cmpop = Py_None;
  }

  Py_INCREF(cmp);
  Py_XINCREF(cmpop);
  Py_XDECREF(self->cmp);
  Py_XDECREF(self->cmpop);
  self->cmp = cmp;
  self->cmpop = cmpop;

  Py_BEGIN_ALLOW_THREADS
  result = tcbdbsetcmpfunc(self->bdb,
                           (BDBCMP)TCBDB_cmpfunc, self);
  Py_END_ALLOW_THREADS

  if (!result) {
    tc_Error_SetBDB(self->bdb);
    Py_DECREF(self->cmp);
    Py_XDECREF(self->cmpop);
    self->cmp = self->cmpop = NULL;
    return NULL;
  }
  Py_RETURN_NONE;
}

tc_BDB_TUNE_OR_OPT(tc_BDB_tune, tune, tcbdbtune);

static PyObject *tc_BDB_setcache(tc_BDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  int lcnum, ncnum;
  bool result;
  static char *kwlist[] = {"lcnum", "ncnum", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "ii:setcache", kwlist,
                                   &lcnum, &ncnum)) {
    return NULL;
  }
  Py_BEGIN_ALLOW_THREADS
  result = tcbdbsetcache(self->bdb, lcnum, ncnum);
  Py_END_ALLOW_THREADS

  if (!result) {
    tc_Error_SetBDB(self->bdb);
    return NULL;
  }
  Py_RETURN_NONE;
}

TC_XDB_OPEN(tc_BDB_open,tc_BDB,tc_BDB_new,tcbdbopen,bdb,tc_BDB_dealloc,tc_Error_SetBDB);
TC_BOOL_NOARGS(tc_BDB_close,tc_BDB,tcbdbclose,bdb,tc_Error_SetBDB,bdb);
TC_XDB_PUT(tc_BDB_put, tc_BDB, put, tcbdbput, bdb, tc_Error_SetBDB);
TC_XDB_PUT(tc_BDB_putkeep, tc_BDB, putkeep, tcbdbputkeep, bdb, tc_Error_SetBDB);
TC_XDB_PUT(tc_BDB_putcat, tc_BDB, putcat, tcbdbputcat, bdb, tc_Error_SetBDB);
TC_XDB_PUT(tc_BDB_putdup, tc_BDB, putdup, tcbdbputdup, bdb, tc_Error_SetBDB);

static PyObject *tc_BDB_putlist(tc_BDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  bool result;
  char *key;
  TCLIST *tcvalue;
  PyObject *value;
  int key_len, value_size, i;
  static char *kwlist[] = {"key", "value", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#O!:putlist", kwlist,
                                   &key, &key_len, &PyList_Type, &value) ||
      !(tcvalue = tclistnew())) {
    return NULL;
  }

  value_size = PyList_Size(value);
  for (i = 0; i < value_size; i++) {
    PyObject *v = PyList_GetItem(value, i);
    if (PyBytes_Check(v)) {
      tclistpush(tcvalue, PyBytes_AsString(v), PyBytes_Size(v));
    }
  }
  Py_BEGIN_ALLOW_THREADS
  result = tcbdbputdup3(self->bdb, key, key_len, tcvalue);
  Py_END_ALLOW_THREADS
  tclistdel(tcvalue);

  if (!result) {
    tc_Error_SetBDB(self->bdb);
    return NULL;
  }
  Py_RETURN_NONE;
}

TC_BOOL_KEYARGS(tc_BDB_out,tc_BDB,out,tcbdbout,bdb,tc_Error_SetBDB,bdb);
TC_BOOL_KEYARGS(tc_BDB_outlist,tc_BDB,outlist,tcbdbout3,bdb,tc_Error_SetBDB,bdb);
TC_STRINGL_KEYARGS(tc_BDB_get,tc_BDB,get,tcbdbget,bdb,tc_Error_SetBDB);

static PyObject *tc_BDB_getlist(tc_BDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  char *key;
  int key_len;
  TCLIST *list;
  static char *kwlist[] = {"key", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#:getlist", kwlist,
                                   &key, &key_len)) {
    return NULL;
  }
  Py_BEGIN_ALLOW_THREADS
  list = tcbdbget4(self->bdb, key, key_len);
  Py_END_ALLOW_THREADS

  TCLIST2PyList()
}

TC_INT_KEYARGS(tc_BDB_vnum,tc_BDB,vnum,tcbdbvnum,bdb,tc_Error_SetBDB);
TC_INT_KEYARGS(tc_BDB_vsiz,tc_BDB,vsiz,tcbdbvsiz,bdb,tc_Error_SetBDB);
TC_BOOL_NOARGS(tc_BDB_sync,tc_BDB,tcbdbsync,bdb,tc_Error_SetBDB,bdb);
tc_BDB_TUNE_OR_OPT(tc_BDB_optimize, optimize, tcbdboptimize);
TC_BOOL_PATHARGS(tc_BDB_copy, tc_BDB, copy, tcbdbcopy, bdb, tc_Error_SetBDB);
TC_BOOL_NOARGS(tc_BDB_vanish,tc_BDB,tcbdbvanish,bdb,tc_Error_SetBDB,bdb);
TC_BOOL_NOARGS(tc_BDB_tranbegin,tc_BDB,tcbdbtranbegin,bdb,tc_Error_SetBDB,bdb);
TC_BOOL_NOARGS(tc_BDB_trancommit,tc_BDB,tcbdbtrancommit,bdb,tc_Error_SetBDB,bdb);
TC_BOOL_NOARGS(tc_BDB_tranabort,tc_BDB,tcbdbtranabort,bdb,tc_Error_SetBDB,bdb);
TC_STRING_NOARGS(tc_BDB_path,tc_BDB,tcbdbpath,bdb,tc_Error_SetBDB);
TC_U_LONG_LONG_NOARGS(tc_BDB_rnum, tc_BDB, tcbdbrnum, bdb, tcbdbecode, tc_Error_SetBDB);
TC_U_LONG_LONG_NOARGS(tc_BDB_fsiz, tc_BDB, tcbdbrnum, bdb, tcbdbecode, tc_Error_SetBDB);

static PyObject *tc_BDB_range(tc_BDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  TCLIST *list;
  char *bkey, *ekey;
  int bkey_len, binc, ekey_len, einc, max;
  static char *kwlist[] = {"bkey", "binc", "ekey", "einc", "max", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "z#iz#ii:range", kwlist,
                                   &bkey, &bkey_len, &binc,
                                   &ekey, &ekey_len, &einc, &max)) {
    return NULL;
  }
  Py_BEGIN_ALLOW_THREADS
  list = tcbdbrange(self->bdb, bkey, bkey_len, binc,
                               ekey, ekey_len, einc, max);
  Py_END_ALLOW_THREADS

  TCLIST2PyList()
}

static PyObject *tc_BDB_rangefwm(tc_BDB *self, PyObject *args, PyObject *keywds) {
  log_trace("ENTER");
  int max;
  TCLIST *list;
  char *prefix;
  static char *kwlist[] = {"prefix", "max", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "si:rangefwm", kwlist,
                                   &prefix, &max)) {
    return NULL;
  }
  Py_BEGIN_ALLOW_THREADS
  list = tcbdbrange3(self->bdb, prefix, max);
  Py_END_ALLOW_THREADS

  TCLIST2PyList()
}

/* TODO: features for experts */

TC_XDB_Contains(tc_BDB_Contains,tc_BDB,tcbdbvsiz,bdb);
TC_XDB___contains__(tc_BDB___contains__,tc_BDB,tc_BDB_Contains);
TC_XDB___getitem__(tc_BDB___getitem__,tc_BDB,tcbdbget,bdb,tc_Error_SetBDB);

static PyObject *tc_BDB_curnew(tc_BDB *self) {
  log_trace("ENTER");
  tc_BDBCursor *cur;
  PyObject *args;
  
  args = Py_BuildValue("(O)", self);
  cur = (tc_BDBCursor *)tc_BDBCursor_new(&tc_BDBCursorType, args, NULL);
  Py_DECREF(args);

  if (cur) {
    return (PyObject *)cur;
  }
  tc_Error_SetBDB(self->bdb);
  return NULL;
}

static PyObject *tc_BDB_GetIter(tc_BDB *self, tc_itertype_t itype) {
  log_trace("ENTER");
  tc_BDBCursor *cur;
  if ((cur = (tc_BDBCursor *)tc_BDB_curnew(self))) {
    cur->itype = itype;
    if (tc_BDBCursor_first(cur)) {
      return (PyObject *)cur;
    }
    tc_BDBCursor_dealloc(cur);
  }
  return NULL;
}

TC_XDB_iters(tc_BDB,tc_BDB_GetIter_keys,tc_BDB_GetIter_values,tc_BDB_GetIter_items,tc_BDB_GetIter);

TC_XDB_rnum(TCBDB_rnum,TCBDB,tcbdbrnum);

static PyObject *tc_BDB_keys(tc_BDB *self) {
  log_trace("ENTER");
  int i;
  BDBCUR *cur;
  PyObject *ret;
  bool result;

  Py_BEGIN_ALLOW_THREADS
  cur = tcbdbcurnew(self->bdb);
  Py_END_ALLOW_THREADS

  if (!cur) {
    return NULL;
  }

  Py_BEGIN_ALLOW_THREADS
  result = tcbdbcurfirst(cur);
  Py_END_ALLOW_THREADS

  if (!result || !(ret = PyList_New(TCBDB_rnum(self->bdb)))) {
    tcbdbcurdel(cur);
    return NULL;
  }
  for (i = 0; result; i++) {
    char *key;
    int key_len;
    PyObject *_key;

    Py_BEGIN_ALLOW_THREADS
    key = tcbdbcurkey(cur, &key_len);
    Py_END_ALLOW_THREADS

    if (!key) { break; }
    _key = PyBytes_FromStringAndSize(key, key_len);
    free(key);
    if (!_key) {
      Py_DECREF(ret);
      return NULL;
    }
    PyList_SET_ITEM(ret, i, _key);

    Py_BEGIN_ALLOW_THREADS
    result = tcbdbcurnext(cur);
    Py_END_ALLOW_THREADS
  }
  tcbdbcurdel(cur);
  return ret;
}

static PyObject *tc_BDB_items(tc_BDB *self) {
  log_trace("ENTER");
  int i;
  bool result;
  BDBCUR *cur;
  PyObject *ret;
  TCXSTR *key, *value;

  Py_BEGIN_ALLOW_THREADS
  cur = tcbdbcurnew(self->bdb);
  Py_END_ALLOW_THREADS

  if (cur) {
    Py_BEGIN_ALLOW_THREADS
    result = tcbdbcurfirst(cur);
    Py_END_ALLOW_THREADS
    if (result) {
      if ((key = tcxstrnew())) {
        if ((value = tcxstrnew())) {
          if ((ret = PyList_New(TCBDB_rnum(self->bdb)))) {
            goto main;
          }
          tcxstrdel(value);
        }
        tcxstrdel(key);
      }
    }
    tcbdbcurdel(cur);
  }
  return NULL;
main:
  for (i = 0; result; i++) {
    Py_BEGIN_ALLOW_THREADS
    result = tcbdbcurrec(cur, key, value);
    Py_END_ALLOW_THREADS
    if (result) {
      PyObject *tuple;
      tuple = Py_BuildValue("(s#s#)", tcxstrptr(key), tcxstrsize(key),
                                      tcxstrptr(value), tcxstrsize(value));
      if (tuple) {
        PyList_SET_ITEM(ret, i, tuple);
        Py_BEGIN_ALLOW_THREADS
        result = tcbdbcurnext(cur);
        Py_END_ALLOW_THREADS
        tcxstrclear(key);
        tcxstrclear(value);
      } else {
        break;
      }
    }
  }
  tcxstrdel(key);
  tcxstrdel(value);
  tcbdbcurdel(cur);
  return ret;
}

static PyObject *tc_BDB_values(tc_BDB *self) {
  log_trace("ENTER");
  int i;
  BDBCUR *cur;
  PyObject *ret;
  bool result;

  Py_BEGIN_ALLOW_THREADS
  cur = tcbdbcurnew(self->bdb);
  Py_END_ALLOW_THREADS

  if (!cur) {
    return NULL;
  }

  Py_BEGIN_ALLOW_THREADS
  result = tcbdbcurfirst(cur);
  Py_END_ALLOW_THREADS

  if (!result || !(ret = PyList_New(TCBDB_rnum(self->bdb)))) {
    tcbdbcurdel(cur);
    return NULL;
  }
  for (i = 0; result; i++) {
    char *value;
    int value_len;
    PyObject *_value;

    Py_BEGIN_ALLOW_THREADS
    value = tcbdbcurval(cur, &value_len);
    Py_END_ALLOW_THREADS

    if (!value) { break; }
    _value = PyBytes_FromStringAndSize(value, value_len);
    free(value);
    if (!_value) {
      Py_DECREF(ret);
      return NULL;
    }
    PyList_SET_ITEM(ret, i, _value);

    Py_BEGIN_ALLOW_THREADS
    result = tcbdbcurnext(cur);
    Py_END_ALLOW_THREADS
  }
  tcbdbcurdel(cur);
  return ret;
}

TC_XDB_length(tc_BDB_length,tc_BDB,TCBDB_rnum,bdb);
TC_XDB_subscript(tc_BDB_subscript,tc_BDB,tcbdbget,bdb,tc_Error_SetBDB);
TC_XDB_DelItem(tc_BDB_DelItem,tc_BDB,tcbdbout,bdb,tc_Error_SetBDB);
TC_XDB_SetItem(tc_BDB_SetItem,tc_BDB,tcbdbput,bdb,tc_Error_SetBDB);
TC_XDB_ass_sub(tc_BDB_ass_sub,tc_BDB,tc_BDB_SetItem,tc_BDB_DelItem);

TC_XDB_addint(tc_BDB_addint,tc_BDB,addint,tcbdbaddint,bdb,tc_Error_SetBDB);
TC_XDB_adddouble(tc_BDB_adddouble,tc_BDB,addint,tcbdbadddouble,bdb,tc_Error_SetBDB);

static PyMethodDef tc_BDB_methods[] = {
  {"errmsg", (PyCFunction)tc_BDB_errmsg, METH_VARARGS | METH_KEYWORDS | METH_CLASS,
    "Get the message string corresponding to an error code."},
  {"ecode", (PyCFunction)tc_BDB_ecode, METH_NOARGS,
    "Get the last happened error code of a B+ tree database object."},
  {"setcmpfunc", (PyCFunction)tc_BDB_setcmpfunc, METH_VARARGS | METH_KEYWORDS,
    "Set the custom comparison function of a B+ tree database object."},
  {"setmutex", (PyCFunction)tc_BDB_setmutex, METH_NOARGS,
    "Set mutual exclusion control of a B+ tree database object for threading."},
  {"tune", (PyCFunction)tc_BDB_tune, METH_VARARGS | METH_KEYWORDS,
    "Set the tuning parameters of a B+ tree database object."},
  {"setcache", (PyCFunction)tc_BDB_setcache, METH_VARARGS | METH_KEYWORDS,
    "Set the caching parameters of a B+ tree database object."},
  {"open", (PyCFunction)tc_BDB_open, METH_VARARGS | METH_KEYWORDS,
    "Open a database file and connect a B+ tree database object."},
  {"close", (PyCFunction)tc_BDB_close, METH_NOARGS,
    "Close a B+ tree database object."},
  {"put", (PyCFunction)tc_BDB_put, METH_VARARGS | METH_KEYWORDS,
    "Store a record into a B+ tree database object."},
  {"putkeep", (PyCFunction)tc_BDB_putkeep, METH_VARARGS | METH_KEYWORDS,
    "Store a new record into a B+ tree database object."},
  {"putcat", (PyCFunction)tc_BDB_putcat, METH_VARARGS | METH_KEYWORDS,
    "Concatenate a value at the end of the existing record in a B+ tree database object."},
  {"putdup", (PyCFunction)tc_BDB_putdup, METH_VARARGS | METH_KEYWORDS,
    "Store a record into a B+ tree database object with allowing duplication of keys."},
  {"putlist", (PyCFunction)tc_BDB_putlist, METH_VARARGS | METH_KEYWORDS,
    "Store records into a B+ tree database object with allowing duplication of keys."},
  {"out", (PyCFunction)tc_BDB_out, METH_VARARGS | METH_KEYWORDS,
    "Remove a record of a B+ tree database object."},
  {"outlist", (PyCFunction)tc_BDB_outlist, METH_VARARGS | METH_KEYWORDS,
    "Remove records of a B+ tree database object."},
  {"get", (PyCFunction)tc_BDB_get, METH_VARARGS | METH_KEYWORDS,
    "Retrieve a record in a B+ tree database object.\n"
   "If the key of duplicated records is specified, the value of the first record is selected."},
  {"getlist", (PyCFunction)tc_BDB_getlist, METH_VARARGS | METH_KEYWORDS,
    "Retrieve records in a B+ tree database object."},
  {"vnum", (PyCFunction)tc_BDB_vnum, METH_VARARGS | METH_KEYWORDS,
    "Get the number of records corresponding a key in a B+ tree database object."},
  {"vsiz", (PyCFunction)tc_BDB_vsiz, METH_VARARGS | METH_KEYWORDS,
    "Get the size of the value of a record in a B+ tree database object."},
  {"sync", (PyCFunction)tc_BDB_sync, METH_NOARGS,
    "Synchronize updated contents of a B+ tree database object with the file and the device."},
  {"optimize", (PyCFunction)tc_BDB_optimize, METH_VARARGS | METH_KEYWORDS,
    "Optimize the file of a B+ tree database object."},
  {"vanish", (PyCFunction)tc_BDB_vanish, METH_NOARGS,
    "Remove all records of a hash database object."},
  {"copy", (PyCFunction)tc_BDB_copy, METH_VARARGS | METH_KEYWORDS,
    "Copy the database file of a B+ tree database object."},
  {"tranbegin", (PyCFunction)tc_BDB_tranbegin, METH_NOARGS,
    "Begin the transaction of a B+ tree database object."},
  {"trancommit", (PyCFunction)tc_BDB_trancommit, METH_NOARGS,
    "Commit the transaction of a B+ tree database object."},
  {"tranabort", (PyCFunction)tc_BDB_tranabort, METH_NOARGS,
    "Abort the transaction of a B+ tree database object."},
  {"path", (PyCFunction)tc_BDB_path, METH_NOARGS,
    "Get the file path of a hash database object."},
  {"rnum", (PyCFunction)tc_BDB_rnum, METH_NOARGS,
    "Get the number of records of a B+ tree database object."},
  {"fsiz", (PyCFunction)tc_BDB_fsiz, METH_NOARGS,
    "Get the size of the database file of a B+ tree database object."},
  {"curnew", (PyCFunction)tc_BDB_curnew, METH_NOARGS,
    "Create a cursor object."},
  {"range", (PyCFunction)tc_BDB_range, METH_VARARGS | METH_KEYWORDS,
    NULL},
  {"rangefwm", (PyCFunction)tc_BDB_rangefwm, METH_VARARGS | METH_KEYWORDS,
    NULL},
  {"__contains__", (PyCFunction)tc_BDB___contains__, METH_O | METH_COEXIST,
    NULL},
  {"__getitem__", (PyCFunction)tc_BDB___getitem__, METH_O | METH_COEXIST,
    NULL},
  {"has_key", (PyCFunction)tc_BDB___contains__, METH_O,
    NULL},
  {"keys", (PyCFunction)tc_BDB_keys, METH_NOARGS,
    NULL},
  {"items", (PyCFunction)tc_BDB_items, METH_NOARGS,
    NULL},
  {"values", (PyCFunction)tc_BDB_values, METH_NOARGS,
    NULL},
  {"iteritems", (PyCFunction)tc_BDB_GetIter_items, METH_NOARGS,
    NULL},
  {"iterkeys", (PyCFunction)tc_BDB_GetIter_keys, METH_NOARGS,
    NULL},
  {"itervalues", (PyCFunction)tc_BDB_GetIter_values, METH_NOARGS,
    NULL},
  {"addint", (PyCFunction)tc_BDB_addint, METH_VARARGS | METH_KEYWORDS,
    "Add an integer to a record in a B+ tree database object."},
  {"adddouble", (PyCFunction)tc_BDB_adddouble, METH_VARARGS | METH_KEYWORDS,
    "Add a real number to a record in a B+ tree database object."},
  {NULL, NULL, 0, NULL}
};

/* Hack to implement "key in dict" */
static PySequenceMethods tc_BDB_as_sequence = {
  0,                             /* sq_length */
  0,                             /* sq_concat */
  0,                             /* sq_repeat */
  0,                             /* sq_item */
  0,                             /* sq_slice */
  0,                             /* sq_ass_item */
  0,                             /* sq_ass_slice */
  (objobjproc)tc_BDB_Contains,  /* sq_contains */
  0,                             /* sq_inplace_concat */
  0,                             /* sq_inplace_repeat */
};

static PyMappingMethods tc_BDB_as_mapping = {
  (lenfunc)tc_BDB_length, /* mp_length */
  (binaryfunc)tc_BDB_subscript, /* mp_subscript */
  (objobjargproc)tc_BDB_ass_sub, /* mp_ass_subscript */
};

/* type objects */

PyTypeObject tc_BDBType = {
  #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(NULL)
    0,                  /*ob_size*/
  #else
    PyVarObject_HEAD_INIT(NULL, 0)
  #endif
  "tc.BDB",                                  /* tp_name */
  sizeof(tc_BDB),                             /* tp_basicsize */
  0,                                           /* tp_itemsize */
  (destructor)tc_BDB_dealloc,                 /* tp_dealloc */
  0,                                           /* tp_print */
  0,                                           /* tp_getattr */
  0,                                           /* tp_setattr */
  0,                                           /* tp_compare */
  0,                                           /* tp_repr */
  0,                                           /* tp_as_number */
  &tc_BDB_as_sequence,                        /* tp_as_sequence */
  &tc_BDB_as_mapping,                         /* tp_as_mapping */
  tc_BDB_Hash,                                /* tp_hash  */
  0,                                           /* tp_call */
  0,                                           /* tp_str */
  0,                                           /* tp_getattro */
  0,                                           /* tp_setattro */
  0,                                           /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
  "Tokyo Cabinet B+ tree database",            /* tp_doc */
  0,                                           /* tp_traverse */
  0,                                           /* tp_clear */
  0,                                           /* tp_richcompare */
  0,                                           /* tp_weaklistoffset */
  (getiterfunc)tc_BDB_GetIter_keys,           /* tp_iter */
  0,                                           /* tp_iternext */
  tc_BDB_methods,                             /* tp_methods */
  0,                                           /* tp_members */
  0,                                           /* tp_getset */
  0,                                           /* tp_base */
  0,                                           /* tp_dict */
  0,                                           /* tp_descr_get */
  0,                                           /* tp_descr_set */
  0,                                           /* tp_dictoffset */
  0,                                           /* tp_init */
  0,                                           /* tp_alloc */
  tc_BDB_new,                                 /* tp_new */
};

int tc_BDB_register(PyObject *module) {
  log_trace("ENTER");
  if (PyType_Ready(&tc_BDBType) == 0)
    return PyModule_AddObject(module, "BDB", (PyObject *)&tc_BDBType);
  return -1;
}
