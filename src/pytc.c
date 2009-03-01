/* Copyright(C) 2007- Tasuku SUENAGA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <Python.h>
#include <tcbdb.h>
#include <tchdb.h>
#include <tcutil.h>
#include <limits.h>

#if PY_VERSION_HEX < 0x02050000
typedef inquiry lenfunc;
#endif

/* FIXME: handle error */
/* FIXME: refactoring */
/* FIXME: setcmpfunc tcbdbcmplexical/decimal/cmpint32/cmpint64' */

typedef enum {
  iter_key = 0,
  iter_value,
  iter_item
} itertype;

static bool
char_bounds(short x) {
  if (x < SCHAR_MIN) {
    PyErr_SetString(PyExc_OverflowError,
    "signed byte integer is less than minimum");
    return false;
  } else if (x > SCHAR_MAX) {
    PyErr_SetString(PyExc_OverflowError,
    "signed byte integer is greater than maximum");
    return false;
  } else {
    return true;
  }
}

/* Error Objects */

static PyObject *PyTCError;

static void
raise_pytc_error(int ecode, const char *errmsg) {
  PyObject *obj;

  obj = Py_BuildValue("(is)", ecode, errmsg);
  PyErr_SetObject(PyTCError, obj);
  Py_DECREF(obj);
}

static void
raise_tchdb_error(TCHDB *hdb) {
  int ecode = tchdbecode(hdb);
  const char *errmsg = tchdberrmsg(ecode);
  if (ecode == TCENOREC) {
    PyErr_SetString(PyExc_KeyError, errmsg);
  } else {
    raise_pytc_error(ecode, errmsg);
  }
}

static void
raise_tcbdb_error(TCBDB *bdb) {
  int ecode = tcbdbecode(bdb);
  const char *errmsg = tcbdberrmsg(ecode);
  if (ecode == TCENOREC) {
    PyErr_SetString(PyExc_KeyError, errmsg);
  } else {
    raise_pytc_error(ecode, errmsg);
  }
}

/* Objects */

typedef struct {
  PyObject_HEAD
  TCHDB	*hdb;
  itertype itype;
  bool hold_itype;
} PyTCHDB;

typedef struct {
  PyObject_HEAD
  TCBDB	*bdb;
  PyObject *cmp;
  PyObject *cmpop;
} PyTCBDB;

typedef struct {
  PyObject_HEAD
  PyTCBDB *bdb;
  BDBCUR *cur;
  itertype itype;
} PyBDBCUR;

/* Macros */

#define BOOL_NOARGS(func,type,call,member,err,errmember) \
  static PyObject * \
  func(type *self) { \
    bool result; \
  \
    Py_BEGIN_ALLOW_THREADS \
    result = call(self->member); \
    Py_END_ALLOW_THREADS \
  \
    if (!result) { \
      err(self->errmember); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }

/* NOTE: this function *doesn't* dealloc pointer returned by tc */
#define STRING_NOARGS(func,type,call,member,err) \
  static PyObject * \
  func(type *self) { \
    const char *str; \
    PyObject *ret; \
  \
    Py_BEGIN_ALLOW_THREADS \
    str = call(self->member); \
    Py_END_ALLOW_THREADS \
  \
    if (!str) { \
      err(self->member); \
      return NULL; \
    } \
    ret = PyString_FromString(str); \
    return ret; \
  } \

/* NOTE: this function dealloc pointer returned by tc */
#define STRINGL_NOARGS(func,type,call,member,err,errmember) \
  static PyObject * \
  func(type *self) { \
    char *str; \
    int str_len; \
    PyObject *ret; \
  \
    Py_BEGIN_ALLOW_THREADS \
    str = call(self->member, &str_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!str) { \
      err(self->errmember); \
      return NULL; \
    } \
    ret = PyString_FromStringAndSize(str, str_len); \
    free(str); \
    return ret; \
  } \

#define PY_NUM_NOARGS(func,type,rettype,call,member,ecode,err,conv) \
  static PyObject * \
  func(type *self) { \
    rettype val; \
  \
    Py_BEGIN_ALLOW_THREADS \
    val = call(self->member); \
    Py_END_ALLOW_THREADS \
  \
    if (ecode(self->member)) { \
      err(self->member); \
      return NULL; \
    } \
    return conv(val); \
  }

#define PY_U_LONG_LONG_NOARGS(func,type,call,member,ecode,err) \
  PY_NUM_NOARGS(func, type, unsigned PY_LONG_LONG, call, member, ecode, err, \
                PyLong_FromUnsignedLongLong)

#define BOOL_KEYARGS(func,type,method,call,member,error,errmember) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    char *key; \
    int key_len; \
    bool result; \
    static char *kwlist[] = {"key", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#:" #method, kwlist, \
                                     &key, &key_len)) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    result = call(self->member, key, key_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!result) { \
      error(self->errmember); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }

#define INT_KEYARGS(func,type,method,call,member,error) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    char *key; \
    int key_len, ret; \
    static char *kwlist[] = {"key", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#:" #method, kwlist, \
                                     &key, &key_len)) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    ret = call(self->member, key, key_len); \
    Py_END_ALLOW_THREADS \
  \
    if (ret == -1) { \
      error(self->member); \
      return NULL; \
    } \
    return PyInt_FromLong((long)ret); \
  }

/* NOTE: this function dealloc pointer returned by tc */
#define STRINGL_KEYARGS(func,type,method,call,member,error) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    PyObject *ret; \
    char *key, *value; \
    int key_len, value_len; \
    static char *kwlist[] = {"key", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#:" #method, kwlist, \
                                     &key, &key_len)) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    value = call(self->member, key, key_len, &value_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!value) { \
      error(self->member); \
      return NULL; \
    } \
    ret = PyString_FromStringAndSize(value, value_len); \
    free(value); \
    return ret; \
  }

#define PyTCXDB_PUT(func,type,method,call,member,error) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    bool result; \
    char *key, *value; \
    int key_len, value_len; \
    static char *kwlist[] = {"key", "value", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s#s#:" #method, kwlist, \
                                     &key, &key_len, \
                                     &value, &value_len)) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    result = call(self->member, key, key_len, value, value_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!result) { \
      error(self->member); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }

#define PyTCXDB_OPEN(func,type,call_new,call_open,member,call_dealloc,error) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    int omode; \
    char *path; \
    bool result; \
    static char *kwlist[] = {"path", "omode", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "si:open", kwlist, \
                                     &path, &omode)) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    result = call_open(self->member, path, omode); \
    Py_END_ALLOW_THREADS \
    if (!result) { \
      error(self->member); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }

#define BOOL_PATHARGS(func,type,method,call,member,error) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    char *str; \
    bool result; \
    static char *kwlist[] = {"path", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "s:" #method, kwlist, \
                                     &str)) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    result = call(self->member, str); \
    Py_END_ALLOW_THREADS \
  \
    if (!result) { \
      error(self->member); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }

/* for internal use */
#define TCXDB_rnum(func,type,call) \
  static uint64_t \
  func(type *obj) { \
    uint64_t ret; \
    Py_BEGIN_ALLOW_THREADS \
    ret = call(obj); \
    Py_END_ALLOW_THREADS \
    return ret; \
  }

/* for dict like interface */
#define TCXDB_length(func,type,call,member) \
  static long \
  func(type *self) { \
    return call(self->member); \
  } \

#define TCXDB_subscript(func,type,call,member,err) \
  static PyObject * \
  func(type *self, PyObject *_key) { \
    PyObject *ret; \
    char *key, *value; \
    int key_len, value_len; \
  \
    if (!PyString_Check(_key)) { \
      PyErr_SetString(PyExc_TypeError, "only string is allowed in []"); \
      return NULL; \
    } \
    key = PyString_AsString(_key); \
    key_len = PyString_GET_SIZE(_key); \
    if (!key || !key_len) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    value = call(self->member, key, key_len, &value_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!value) { \
      err(self->member); \
      return NULL; \
    } \
    ret = PyString_FromStringAndSize(value, value_len); \
    free(value); \
    return ret; \
  } \

#define TCXDB_DelItem(func,type,call,member,err) \
  int \
  func(type *self, PyObject *_key) { \
    bool result; \
    char *key = PyString_AsString(_key); \
    int key_len = PyString_GET_SIZE(_key); \
  \
    if (!key || !key_len) { \
      return -1; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    result = call(self->member, key, key_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!result) { \
      err(self->member); \
      return -1; \
    } \
    return 0; \
  }

#define TCXDB_SetItem(func,type,call,member,err) \
  int \
  func(type *self, PyObject *_key, PyObject *_value) { \
    bool result; \
    char *key = PyString_AsString(_key), *value = PyString_AsString(_value); \
    int key_len = PyString_GET_SIZE(_key), value_len = PyString_GET_SIZE(_value); \
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

#define TCXDB_ass_sub(func,type,setitem,delitem) \
  static int \
  func(type *self, PyObject *v, PyObject *w) \
  { \
    if (w) { \
      return setitem(self, v, w); \
    } else { \
      return delitem(self, v); \
    } \
  }

#define TCXDB_Contains(func,type,call,member) \
  static int \
  func(type *self, PyObject *_key) { \
    char *key = PyString_AsString(_key); \
    int key_len = PyString_GET_SIZE(_key), value_len; \
  \
    if (!key || !key_len) { \
      return -1; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    value_len = call(self->member, key, key_len); \
    Py_END_ALLOW_THREADS \
  \
    return (value_len != -1); \
  }

#define TCXDB___contains__(func,type,call) \
  static PyObject * \
  func(type *self, PyObject *_key) { \
    return PyBool_FromLong(call(self, _key) != 0); \
  }

#define TCXDB___getitem__(func,type,call,member,err) \
  static PyObject * \
  func(type *self, PyObject *_key) { \
    PyObject *ret; \
    char *key = PyString_AsString(_key), *value; \
    int key_len = PyString_GET_SIZE(_key), value_len; \
  \
    if (!key || !key_len) { \
      return NULL; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    value = call(self->member, key, key_len, &value_len); \
    Py_END_ALLOW_THREADS \
  \
    if (!value) { \
      err(self->member); \
      return NULL; \
    } \
    ret = PyString_FromStringAndSize(value, value_len); \
    free(value); \
    return ret; \
  }

#define TCXDB_iters(type,funcikey,funcival,funciitem,call) \
  static PyObject * \
  funcikey(type *self) { \
    return call(self, iter_key); \
  } \
  \
  static PyObject * \
  funcival(type *self) { \
    return call(self, iter_value); \
  } \
  \
  static PyObject * \
  funciitem(type *self) { \
    return call(self, iter_item); \
  }

/* for TCXSTR key/value */
#define GET_TCXSTR_KEY_VALUE(call,callarg) \
  PyObject *ret = NULL; \
  TCXSTR *key, *value; \
  \
  key = tcxstrnew(); \
  value = tcxstrnew(); \
  if (key && value) { \
    bool result; \
  \
    Py_BEGIN_ALLOW_THREADS \
    result = call(callarg, key, value); \
    Py_END_ALLOW_THREADS

#define CLEAR_TCXSTR_KEY_VALUE() \
  } \
  if (key) { tcxstrdel(key); } \
  if (value) { tcxstrdel(value); } \
  return ret;

#define TCLIST2PyList() \
  if (!list) { \
    raise_tcbdb_error(self->bdb); \
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
        _value = PyString_FromStringAndSize(value, value_len); \
        PyList_SET_ITEM(ret, i, _value); \
      } \
    } \
    tclistdel(list); \
    return ret; \
  }

/*** TCHDB ***/

#define PyTCHDB_TUNE(a,b,c) \
  static PyObject * \
  a(PyTCHDB *self, PyObject *args, PyObject *keywds) { \
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
      raise_tchdb_error(self->hdb); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }

static long
PyTCHDB_Hash(PyObject *self) {
  PyErr_SetString(PyExc_TypeError, "HDB objects are unhashable");
  return -1;
}

static PyObject *
PyTCHDB_errmsg(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  int ecode;
  static char *kwlist[] = {"ecode", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "i:errmsg", kwlist,
                                   &ecode)) {
    return NULL;
  }
  return PyString_FromString(tchdberrmsg(ecode));
}

static void
PyTCHDB_dealloc(PyTCHDB *self)
{
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
  self->ob_type->tp_free(self);
}

static PyObject *
PyTCHDB_new(PyTypeObject *type, PyObject *args, PyObject *keywds)
{
  PyTCHDB *self;
  if (!(self = (PyTCHDB *)type->tp_alloc(type, 0))) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc PyTCHDB instance");
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
      raise_tchdb_error(self->hdb);
    }
  } else {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc TCHDB instance");
  }
  PyTCHDB_dealloc(self);
  return NULL;
}

static PyObject *
PyTCHDB_ecode(PyTCHDB *self) {
  return PyInt_FromLong((long)tchdbecode(self->hdb));
}

BOOL_NOARGS(PyTCHDB_setmutex,PyTCHDB,tchdbsetmutex,hdb,raise_tchdb_error,hdb)
PyTCHDB_TUNE(PyTCHDB_tune, tune, tchdbtune)
PyTCXDB_OPEN(PyTCHDB_open,PyTCHDB,PyTCHDB_new,tchdbopen,hdb,PyTCHDB_dealloc,raise_tchdb_error)
BOOL_NOARGS(PyTCHDB_close,PyTCHDB,tchdbclose,hdb,raise_tchdb_error,hdb)
PyTCXDB_PUT(PyTCHDB_put, PyTCHDB, put, tchdbput, hdb, raise_tchdb_error)
PyTCXDB_PUT(PyTCHDB_putkeep, PyTCHDB, putkeep, tchdbputkeep, hdb, raise_tchdb_error)
PyTCXDB_PUT(PyTCHDB_putcat, PyTCHDB, putcat, tchdbputcat, hdb, raise_tchdb_error)
PyTCXDB_PUT(PyTCHDB_putasync, PyTCHDB, putasync, tchdbputasync, hdb, raise_tchdb_error)
BOOL_KEYARGS(PyTCHDB_out,PyTCHDB,out,tchdbout,hdb,raise_tchdb_error,hdb)
STRINGL_KEYARGS(PyTCHDB_get,PyTCHDB,get,tchdbget,hdb,raise_tchdb_error)
INT_KEYARGS(PyTCHDB_vsiz,PyTCHDB,vsiz,tchdbvsiz,hdb,raise_tchdb_error)
BOOL_NOARGS(PyTCHDB_iterinit,PyTCHDB,tchdbiterinit,hdb,raise_tchdb_error,hdb)

static PyObject *
PyTCHDB_GetIter(PyTCHDB *self, itertype itype) {
  if (PyTCHDB_iterinit(self)) {
    Py_INCREF(self);
    /* hack */
    if (self->hold_itype) {
      self->hold_itype = false;
    } else {
      self->itype = itype;
      if (itype != iter_key) {
        self->hold_itype = true;
      }
    }
    return (PyObject *)self;
  }
  return NULL;
}

TCXDB_iters(PyTCHDB,PyTCHDB_GetIter_keys,PyTCHDB_GetIter_values,PyTCHDB_GetIter_items,PyTCHDB_GetIter)

static PyObject *
PyTCHDB_iternext(PyTCHDB *self) {
  if (self->itype == iter_key) {
    void *key;
    int key_len;

    Py_BEGIN_ALLOW_THREADS
    key = tchdbiternext(self->hdb, &key_len);
    Py_END_ALLOW_THREADS

    if (key) {
      PyObject *_key;
      _key = PyString_FromStringAndSize(key, key_len);
      free(key);
      return _key;
    }
    return NULL;
  } else {
    GET_TCXSTR_KEY_VALUE(tchdbiternext3,self->hdb)
    if (result) {
      if (self->itype == iter_value) {
        ret = PyString_FromStringAndSize(tcxstrptr(value), tcxstrsize(value));
      } else {
        ret = Py_BuildValue("(s#s#)", tcxstrptr(key), tcxstrsize(key),
                                      tcxstrptr(value), tcxstrsize(value));
      }
    }
    CLEAR_TCXSTR_KEY_VALUE()
  }
}

BOOL_NOARGS(PyTCHDB_sync,PyTCHDB,tchdbsync,hdb,raise_tchdb_error,hdb)
PyTCHDB_TUNE(PyTCHDB_optimize, optimize, tchdboptimize)
STRING_NOARGS(PyTCHDB_path,PyTCHDB,tchdbpath,hdb,raise_tchdb_error)
PY_U_LONG_LONG_NOARGS(PyTCHDB_rnum, PyTCHDB, tchdbrnum, hdb, tchdbecode, raise_tchdb_error)
PY_U_LONG_LONG_NOARGS(PyTCHDB_fsiz, PyTCHDB, tchdbrnum, hdb, tchdbecode, raise_tchdb_error)
BOOL_NOARGS(PyTCHDB_vanish,PyTCHDB,tchdbvanish,hdb,raise_tchdb_error,hdb)
BOOL_PATHARGS(PyTCHDB_copy, PyTCHDB, copy, tchdbcopy, hdb, raise_tchdb_error)

/* TODO: features for experts */

TCXDB_Contains(PyTCHDB_Contains,PyTCHDB,tchdbvsiz,hdb)
TCXDB___contains__(PyTCHDB___contains__,PyTCHDB,PyTCHDB_Contains)
TCXDB___getitem__(PyTCHDB___getitem__,PyTCHDB,tchdbget,hdb,raise_tchdb_error)
TCXDB_rnum(TCHDB_rnum,TCHDB,tchdbrnum)

static PyObject *
PyTCHDB_keys(PyTCHDB *self) {
  int i;
  PyObject *ret;
  if (!PyTCHDB_iterinit((PyTCHDB *)self) ||
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
    _key = PyString_FromStringAndSize(key, key_len);
    free(key);
    if (!_key) {
      Py_DECREF(ret);
      return NULL;
    }
    PyList_SET_ITEM(ret, i, _key);
  }
  return ret;
}

static PyObject *
PyTCHDB_items(PyTCHDB *self) {
  int i, n = TCHDB_rnum(self->hdb);
  PyObject *ret, *item;
  if (!PyTCHDB_iterinit((PyTCHDB *)self) ||
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
      _key = PyString_FromStringAndSize(key, key_len);
      free(key);
      if (!_key) {
        Py_DECREF(ret);
        return NULL;
      }
      _value = PyString_FromStringAndSize(value, value_len);
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

static PyObject *
PyTCHDB_values(PyTCHDB *self) {
  int i;
  PyObject *ret;
  if (!PyTCHDB_iterinit((PyTCHDB *)self) ||
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
      _value = PyString_FromStringAndSize(value, value_len);
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

TCXDB_length(PyTCHDB_length,PyTCHDB,TCHDB_rnum,hdb)
TCXDB_subscript(PyTCHDB_subscript,PyTCHDB,tchdbget,hdb,raise_tchdb_error)
TCXDB_DelItem(PyTCHDB_DelItem,PyTCHDB,tchdbout,hdb,raise_tchdb_error)
TCXDB_SetItem(PyTCHDB_SetItem,PyTCHDB,tchdbput,hdb,raise_tchdb_error)
TCXDB_ass_sub(PyTCHDB_ass_sub,PyTCHDB,PyTCHDB_SetItem,PyTCHDB_DelItem)

#define TCXDB_addint(func,type,method,call,member,err) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    char *key; \
    int key_len, num; \
  \
    static char *kwlist[] = {"key", "num", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, \
                                     "s#i:" #method, kwlist, \
                                     &key, &key_len, &num)) { \
      return NULL; \
    } \
    if (!key || !key_len) { \
      err(self->member); \
      Py_RETURN_NONE; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    num = call(self->member, key, key_len, num); \
    Py_END_ALLOW_THREADS \
  \
    return Py_BuildValue("i", num); \
  }

#define TCXDB_adddouble(func,type,method,call,member,err) \
  static PyObject * \
  func(type *self, PyObject *args, PyObject *keywds) { \
    char *key; \
    int key_len; \
    double num; \
  \
    static char *kwlist[] = {"key", "num", NULL}; \
  \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, \
                                     "s#d:" #method, kwlist, \
                                     &key, &key_len, &num)) { \
      return NULL; \
    } \
    if (!key || !key_len) { \
      err(self->member); \
      Py_RETURN_NONE; \
    } \
    Py_BEGIN_ALLOW_THREADS \
    num = call(self->member, key, key_len, num); \
    Py_END_ALLOW_THREADS \
  \
    return Py_BuildValue("d", num); \
  }

TCXDB_addint(PyTCHDB_addint,PyTCHDB,addint,tchdbaddint,hdb,raise_tchdb_error)
TCXDB_adddouble(PyTCHDB_adddouble,PyTCHDB,addint,tchdbadddouble,hdb,raise_tchdb_error)

/* methods of classes */
static PyMethodDef PyTCHDB_methods[] = {
  {"errmsg", (PyCFunction)PyTCHDB_errmsg,
   METH_VARARGS | METH_KEYWORDS | METH_CLASS,
   "Get the message string corresponding to an error code."},
  {"ecode", (PyCFunction)PyTCHDB_ecode,
   METH_NOARGS,
   "Get the last happened error code of a hash database object."},
  {"setmutex", (PyCFunction)PyTCHDB_setmutex,
   METH_NOARGS,
   "Set mutual exclusion control of a hash database object for threading."},
  {"tune", (PyCFunction)PyTCHDB_tune,
   METH_VARARGS | METH_KEYWORDS,
   "Set the tuning parameters of a hash database object."},
  {"open", (PyCFunction)PyTCHDB_open,
   METH_VARARGS | METH_KEYWORDS,
   "Open a database file and connect a hash database object."},
  {"close", (PyCFunction)PyTCHDB_close,
   METH_NOARGS,
   "Close a hash database object."},
  {"put", (PyCFunction)PyTCHDB_put,
   METH_VARARGS | METH_KEYWORDS,
   "Store a record into a hash database object."},
  {"putkeep", (PyCFunction)PyTCHDB_putkeep,
   METH_VARARGS | METH_KEYWORDS,
   "Store a new record into a hash database object."},
  {"putcat", (PyCFunction)PyTCHDB_putcat,
   METH_VARARGS | METH_KEYWORDS,
   "Concatenate a value at the end of the existing record in a hash database object."},
  {"putasync", (PyCFunction)PyTCHDB_putasync,
   METH_VARARGS | METH_KEYWORDS,
   "Store a record into a hash database object in asynchronous fashion."},
  {"out", (PyCFunction)PyTCHDB_out,
   METH_VARARGS | METH_KEYWORDS,
   "Remove a record of a hash database object."},
  {"get", (PyCFunction)PyTCHDB_get,
   METH_VARARGS | METH_KEYWORDS,
   "Retrieve a record in a hash database object."},
  {"vsiz", (PyCFunction)PyTCHDB_vsiz,
   METH_VARARGS | METH_KEYWORDS,
   "Get the size of the value of a record in a hash database object."},
  {"iterinit", (PyCFunction)PyTCHDB_iterinit,
   METH_NOARGS,
   "Initialize the iterator of a hash database object."},
  {"iternext", (PyCFunction)PyTCHDB_iternext,
   METH_NOARGS,
   "Get the next extensible objects of the iterator of a hash database object."},
  {"sync", (PyCFunction)PyTCHDB_sync,
   METH_NOARGS,
   "Synchronize updated contents of a hash database object with the file and the device."},
  {"optimize", (PyCFunction)PyTCHDB_optimize,
   METH_VARARGS | METH_KEYWORDS,
   "Optimize the file of a hash database object."},
  {"vanish", (PyCFunction)PyTCHDB_vanish,
   METH_NOARGS,
   "Remove all records of a hash database object."},
  {"path", (PyCFunction)PyTCHDB_path,
   METH_NOARGS,
   "Get the file path of a hash database object."},
  {"copy", (PyCFunction)PyTCHDB_copy,
   METH_VARARGS | METH_KEYWORDS,
   "Copy the database file of a hash database object."},
  {"rnum", (PyCFunction)PyTCHDB_rnum,
   METH_NOARGS,
   "Get the number of records of a hash database object."},
  {"fsiz", (PyCFunction)PyTCHDB_fsiz,
   METH_NOARGS,
   "Get the size of the database file of a hash database object."},
  {"__contains__", (PyCFunction)PyTCHDB___contains__,
   METH_O | METH_COEXIST,
   ""},
  {"__getitem__", (PyCFunction)PyTCHDB___getitem__,
   METH_O | METH_COEXIST,
   ""},
  {"has_key", (PyCFunction)PyTCHDB___contains__,
   METH_O,
   ""},
  {"keys", (PyCFunction)PyTCHDB_keys,
   METH_NOARGS,
   ""},
  {"items", (PyCFunction)PyTCHDB_items,
   METH_NOARGS,
   ""},
  {"values", (PyCFunction)PyTCHDB_values,
   METH_NOARGS,
   ""},
  {"iteritems", (PyCFunction)PyTCHDB_GetIter_items,
   METH_NOARGS,
   ""},
  {"iterkeys", (PyCFunction)PyTCHDB_GetIter_keys,
   METH_NOARGS,
   ""},
  {"itervalues", (PyCFunction)PyTCHDB_GetIter_values,
   METH_NOARGS,
   ""},
  {"addint", (PyCFunction)PyTCHDB_addint,
   METH_VARARGS | METH_KEYWORDS,
   "Add an integer to a record in a hash database object."},
  {"adddouble", (PyCFunction)PyTCHDB_adddouble,
   METH_VARARGS | METH_KEYWORDS,
   "Add a real number to a record in a hash database object."},
  /*
  {"", (PyCFunction)PyTCHDB_,
   METH_VARARGS | METH_KEYWORDS,
   ""},
  */
  {NULL, NULL, 0, NULL}
};


/* Hack to implement "key in dict" */
static PySequenceMethods PyTCHDB_as_sequence = {
  0,                             /* sq_length */
  0,                             /* sq_concat */
  0,                             /* sq_repeat */
  0,                             /* sq_item */
  0,                             /* sq_slice */
  0,                             /* sq_ass_item */
  0,                             /* sq_ass_slice */
  (objobjproc)PyTCHDB_Contains,  /* sq_contains */
  0,                             /* sq_inplace_concat */
  0,                             /* sq_inplace_repeat */
};

static PyMappingMethods PyTCHDB_as_mapping = {
  (lenfunc)PyTCHDB_length, /* mp_length (inquiry/lenfunc )*/
  (binaryfunc)PyTCHDB_subscript, /* mp_subscript */
  (objobjargproc)PyTCHDB_ass_sub, /* mp_ass_subscript */
};

static PyTypeObject PyTCHDB_Type = {
  PyObject_HEAD_INIT(NULL)
  0,                                           /* ob_size */
  "pytc.HDB",                                  /* tp_name */
  sizeof(PyTCHDB),                             /* tp_basicsize */
  0,                                           /* tp_itemsize */
  (destructor)PyTCHDB_dealloc,                 /* tp_dealloc */
  0,                                           /* tp_print */
  0,                                           /* tp_getattr */
  0,                                           /* tp_setattr */
  0,                                           /* tp_compare */
  0,                                           /* tp_repr */
  0,                                           /* tp_as_number */
  &PyTCHDB_as_sequence,                        /* tp_as_sequence */
  &PyTCHDB_as_mapping,                         /* tp_as_mapping */
  PyTCHDB_Hash,                                /* tp_hash  */
  0,                                           /* tp_call */
  0,                                           /* tp_str */
  0,                                           /* tp_getattro */
  0,                                           /* tp_setattro */
  0,                                           /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
  "tchdb",                                     /* tp_doc */
  0,                                           /* tp_traverse */
  0,                                           /* tp_clear */
  0,                                           /* tp_richcompare */
  0,                                           /* tp_weaklistoffset */
  (getiterfunc)PyTCHDB_GetIter_keys,           /* tp_iter */
  (iternextfunc)PyTCHDB_iternext,              /* tp_iternext */
  PyTCHDB_methods,                             /* tp_methods */
  0,                                           /* tp_members */
  0,                                           /* tp_getset */
  0,                                           /* tp_base */
  0,                                           /* tp_dict */
  0,                                           /* tp_descr_get */
  0,                                           /* tp_descr_set */
  0,                                           /* tp_dictoffset */
  0,                                           /* tp_init */
  0,                                           /* tp_alloc */
  PyTCHDB_new,                                 /* tp_new */
};

/* BDBCUR */

static PyTypeObject PyTCBDB_Type;

static PyObject *
PyBDBCUR_new(PyTypeObject *type, PyObject *args, PyObject *keywds)
{
  PyTCBDB *bdb;
  PyBDBCUR *self;
  static char *kwlist[] = {"bdb", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!:new", kwlist,
                                   &PyTCBDB_Type, &bdb)) {
    return NULL;
  }
  if (!(self = (PyBDBCUR *)type->tp_alloc(type, 0))) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc PyBDBCUR instance");
    return NULL;
  }

  Py_BEGIN_ALLOW_THREADS
  self->cur = tcbdbcurnew(bdb->bdb);
  Py_END_ALLOW_THREADS

  if (!self->cur) {
    self->ob_type->tp_free(self);
    raise_tcbdb_error(bdb->bdb);
    return NULL;
  }
  Py_INCREF(bdb);
  self->bdb = bdb;
  return (PyObject *)self;
}

static PyObject *
PyBDBCUR_dealloc(PyBDBCUR *self)
{
  Py_BEGIN_ALLOW_THREADS
  tcbdbcurdel(self->cur);
  Py_END_ALLOW_THREADS
  Py_XDECREF(self->bdb);
  self->ob_type->tp_free(self);
  Py_RETURN_NONE;
}

BOOL_NOARGS(PyBDBCUR_first,PyBDBCUR,tcbdbcurfirst,cur,raise_tcbdb_error,bdb->bdb)
BOOL_NOARGS(PyBDBCUR_last,PyBDBCUR,tcbdbcurlast,cur,raise_tcbdb_error,bdb->bdb)
BOOL_KEYARGS(PyBDBCUR_jump,PyBDBCUR,jump,tcbdbcurjump,cur,raise_tcbdb_error,bdb->bdb)
BOOL_NOARGS(PyBDBCUR_prev,PyBDBCUR,tcbdbcurprev,cur,raise_tcbdb_error,bdb->bdb)
BOOL_NOARGS(PyBDBCUR_next,PyBDBCUR,tcbdbcurnext,cur,raise_tcbdb_error,bdb->bdb)

static PyObject *
PyBDBCUR_put(PyBDBCUR *self, PyObject *args, PyObject *keywds) {
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
    raise_tcbdb_error(self->bdb->bdb);
    return NULL;
  }
  Py_RETURN_NONE;
}

BOOL_NOARGS(PyBDBCUR_out,PyBDBCUR,tcbdbcurout,cur,raise_tcbdb_error,bdb->bdb)
STRINGL_NOARGS(PyBDBCUR_key,PyBDBCUR,tcbdbcurkey,cur,raise_tcbdb_error,bdb->bdb)
STRINGL_NOARGS(PyBDBCUR_val,PyBDBCUR,tcbdbcurval,cur,raise_tcbdb_error,bdb->bdb)

static PyObject *
PyBDBCUR_rec(PyBDBCUR *self) {
  GET_TCXSTR_KEY_VALUE(tcbdbcurrec,self->cur)
  if (result) {
    ret = Py_BuildValue("(s#s#)", tcxstrptr(key), tcxstrsize(key),
                                  tcxstrptr(value), tcxstrsize(value));
  }
  if (!ret) {
    raise_tcbdb_error(self->bdb->bdb);
  }
  CLEAR_TCXSTR_KEY_VALUE()
}

static PyObject *
PyBDBCUR_iternext(PyBDBCUR *self) {
  /* TODO: performance up */
  GET_TCXSTR_KEY_VALUE(tcbdbcurrec,self->cur)
  if (result) {
    switch (self->itype) {
      case iter_key:
        ret = PyString_FromStringAndSize(tcxstrptr(key), tcxstrsize(key));
        break;
      case iter_value:
        ret = PyString_FromStringAndSize(tcxstrptr(value), tcxstrsize(value));
        break;
      case iter_item:
        ret = Py_BuildValue("(s#s#)", tcxstrptr(key), tcxstrsize(key),
                                      tcxstrptr(value), tcxstrsize(value));
        break;
    }
  }
  Py_BEGIN_ALLOW_THREADS
  result = tcbdbcurnext(self->cur);
  Py_END_ALLOW_THREADS
  CLEAR_TCXSTR_KEY_VALUE()
}

TCXDB_addint(PyTCBDB_addint,PyTCBDB,addint,tcbdbaddint,bdb,raise_tcbdb_error)
TCXDB_adddouble(PyTCBDB_adddouble,PyTCBDB,addint,tcbdbadddouble,bdb,raise_tcbdb_error)

static PyMethodDef PyBDBCUR_methods[] = {
  {"first", (PyCFunction)PyBDBCUR_first,
   METH_NOARGS,
   "Move a cursor object to the first record."},
  {"last", (PyCFunction)PyBDBCUR_last,
   METH_NOARGS,
   "Move a cursor object to the last record."},
  {"jump", (PyCFunction)PyBDBCUR_jump,
   METH_VARARGS | METH_KEYWORDS,
   "Move a cursor object to the front of records corresponding a key."},
  {"prev", (PyCFunction)PyBDBCUR_prev,
   METH_NOARGS,
   "Move a cursor object to the previous record."},
  {"next", (PyCFunction)PyBDBCUR_next,
   METH_NOARGS,
   "Move a cursor object to the next record."},
  {"put", (PyCFunction)PyBDBCUR_put,
   METH_VARARGS | METH_KEYWORDS,
   "Insert a record around a cursor object."},
  {"out", (PyCFunction)PyBDBCUR_out,
   METH_NOARGS,
   "Delete the record where a cursor object is."},
  {"key", (PyCFunction)PyBDBCUR_key,
   METH_NOARGS,
   "Get the key of the record where the cursor object is."},
  {"val", (PyCFunction)PyBDBCUR_val,
   METH_NOARGS,
   "Get the value of the record where the cursor object is."},
  {"rec", (PyCFunction)PyBDBCUR_rec,
   METH_NOARGS,
   "Get the key and the value of the record where the cursor object is."},
  /*
  {"", (PyCFunction)PyBDBCUR_,
   METH_VARARGS | METH_KEYWORDS,
   ""},
  */
  {NULL, NULL, 0, NULL}
};

static PyTypeObject PyBDBCUR_Type = {
  PyObject_HEAD_INIT(NULL)
  0,                                           /* ob_size */
  "pytc.BDBCUR",                               /* tp_name */
  sizeof(PyBDBCUR),                            /* tp_basicsize */
  0,                                           /* tp_itemsize */
  (destructor)PyBDBCUR_dealloc,                /* tp_dealloc */
  0,                                           /* tp_print */
  0,                                           /* tp_getattr */
  0,                                           /* tp_setattr */
  0,                                           /* tp_compare */
  0,                                           /* tp_repr */
  0,                                           /* tp_as_number */
  0,                                           /* tp_as_sequence */
  0,                                           /* tp_as_mapping */
  0,                                           /* tp_hash  */
  0,                                           /* tp_call */
  0,                                           /* tp_str */
  0,                                           /* tp_getattro */
  0,                                           /* tp_setattro */
  0,                                           /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
  "bdbcur",                                    /* tp_doc */
  0,                                           /* tp_traverse */
  0,                                           /* tp_clear */
  0,                                           /* tp_richcompare */
  0,                                           /* tp_weaklistoffset */
  PyObject_SelfIter,                           /* tp_iter */
  (iternextfunc)PyBDBCUR_iternext,             /* tp_iternext */
  PyBDBCUR_methods,                            /* tp_methods */
  0,                                           /* tp_members */
  0,                                           /* tp_getset */
  0,                                           /* tp_base */
  0,                                           /* tp_dict */
  0,                                           /* tp_descr_get */
  0,                                           /* tp_descr_set */
  0,                                           /* tp_dictoffset */
  0,                                           /* tp_init */
  0,                                           /* tp_alloc */
  PyBDBCUR_new,                                /* tp_new */
};

/*** TCBDB ***/

#define PyTCBDB_TUNE(a,b,c) \
  static PyObject * \
  a(PyTCBDB *self, PyObject *args, PyObject *keywds) { \
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
      raise_tcbdb_error(self->bdb); \
      return NULL; \
    } \
    Py_RETURN_NONE; \
  }

static long
PyTCBDB_Hash(PyObject *self) {
  PyErr_SetString(PyExc_TypeError, "BDB objects are unhashable");
  return -1;
}

static PyObject *
PyTCBDB_errmsg(PyTypeObject *type, PyObject *args, PyObject *keywds) {
  int ecode;
  static char *kwlist[] = {"ecode", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, keywds, "i:errmsg", kwlist,
                                   &ecode)) {
    return NULL;
  }
  return PyString_FromString(tcbdberrmsg(ecode));
}

static void
PyTCBDB_dealloc(PyTCBDB *self)
{
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
  self->ob_type->tp_free(self);
}

static PyObject *
PyTCBDB_new(PyTypeObject *type, PyObject *args, PyObject *keywds)
{
  PyTCBDB *self;
  if (!(self = (PyTCBDB *)type->tp_alloc(type, 0))) {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc PyTCBDB instance");
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
      raise_tcbdb_error(self->bdb);
    }
  } else {
    PyErr_SetString(PyExc_MemoryError, "Cannot alloc TCBDB instance");
  }
  PyTCBDB_dealloc(self);
  return NULL;
}
static PyObject *
PyTCBDB_ecode(PyTCBDB *self) {
  return PyInt_FromLong((long)tcbdbecode(self->bdb));
}

BOOL_NOARGS(PyTCBDB_setmutex,PyTCBDB,tcbdbsetmutex,bdb,raise_tcbdb_error,bdb)

static int
TCBDB_cmpfunc(const char *aptr, int asiz,
              const char *bptr, int bsiz, PyTCBDB *self) {
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

static PyObject *
PyTCBDB_setcmpfunc(PyTCBDB *self, PyObject *args, PyObject *keywds) {
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
    raise_tcbdb_error(self->bdb);
    Py_DECREF(self->cmp);
    Py_XDECREF(self->cmpop);
    self->cmp = self->cmpop = NULL;
    return NULL;
  }
  Py_RETURN_NONE;
}

PyTCBDB_TUNE(PyTCBDB_tune, tune, tcbdbtune)

static PyObject *
PyTCBDB_setcache(PyTCBDB *self, PyObject *args, PyObject *keywds) {
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
    raise_tcbdb_error(self->bdb);
    return NULL;
  }
  Py_RETURN_NONE;
}

PyTCXDB_OPEN(PyTCBDB_open,PyTCBDB,PyTCBDB_new,tcbdbopen,bdb,PyTCBDB_dealloc,raise_tcbdb_error)
BOOL_NOARGS(PyTCBDB_close,PyTCBDB,tcbdbclose,bdb,raise_tcbdb_error,bdb)
PyTCXDB_PUT(PyTCBDB_put, PyTCBDB, put, tcbdbput, bdb, raise_tcbdb_error)
PyTCXDB_PUT(PyTCBDB_putkeep, PyTCBDB, putkeep, tcbdbputkeep, bdb, raise_tcbdb_error)
PyTCXDB_PUT(PyTCBDB_putcat, PyTCBDB, putcat, tcbdbputcat, bdb, raise_tcbdb_error)
PyTCXDB_PUT(PyTCBDB_putdup, PyTCBDB, putdup, tcbdbputdup, bdb, raise_tcbdb_error)

static PyObject *
PyTCBDB_putlist(PyTCBDB *self, PyObject *args, PyObject *keywds) {
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
    if (PyString_Check(v)) {
      tclistpush(tcvalue, PyString_AsString(v), PyString_Size(v));
    }
  }
  Py_BEGIN_ALLOW_THREADS
  result = tcbdbputdup3(self->bdb, key, key_len, tcvalue);
  Py_END_ALLOW_THREADS
  tclistdel(tcvalue);

  if (!result) {
    raise_tcbdb_error(self->bdb);
    return NULL;
  }
  Py_RETURN_NONE;
}

BOOL_KEYARGS(PyTCBDB_out,PyTCBDB,out,tcbdbout,bdb,raise_tcbdb_error,bdb)
BOOL_KEYARGS(PyTCBDB_outlist,PyTCBDB,outlist,tcbdbout3,bdb,raise_tcbdb_error,bdb)
STRINGL_KEYARGS(PyTCBDB_get,PyTCBDB,get,tcbdbget,bdb,raise_tcbdb_error)

static PyObject *
PyTCBDB_getlist(PyTCBDB *self, PyObject *args, PyObject *keywds) {
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

INT_KEYARGS(PyTCBDB_vnum,PyTCBDB,vnum,tcbdbvnum,bdb,raise_tcbdb_error)
INT_KEYARGS(PyTCBDB_vsiz,PyTCBDB,vsiz,tcbdbvsiz,bdb,raise_tcbdb_error)
BOOL_NOARGS(PyTCBDB_sync,PyTCBDB,tcbdbsync,bdb,raise_tcbdb_error,bdb)
PyTCBDB_TUNE(PyTCBDB_optimize, optimize, tcbdboptimize)
BOOL_PATHARGS(PyTCBDB_copy, PyTCBDB, copy, tcbdbcopy, bdb, raise_tcbdb_error)
BOOL_NOARGS(PyTCBDB_vanish,PyTCBDB,tcbdbvanish,bdb,raise_tcbdb_error,bdb)
BOOL_NOARGS(PyTCBDB_tranbegin,PyTCBDB,tcbdbtranbegin,bdb,raise_tcbdb_error,bdb)
BOOL_NOARGS(PyTCBDB_trancommit,PyTCBDB,tcbdbtrancommit,bdb,raise_tcbdb_error,bdb)
BOOL_NOARGS(PyTCBDB_tranabort,PyTCBDB,tcbdbtranabort,bdb,raise_tcbdb_error,bdb)
STRING_NOARGS(PyTCBDB_path,PyTCBDB,tcbdbpath,bdb,raise_tcbdb_error)
PY_U_LONG_LONG_NOARGS(PyTCBDB_rnum, PyTCBDB, tcbdbrnum, bdb, tcbdbecode, raise_tcbdb_error)
PY_U_LONG_LONG_NOARGS(PyTCBDB_fsiz, PyTCBDB, tcbdbrnum, bdb, tcbdbecode, raise_tcbdb_error)

static PyObject *
PyTCBDB_range(PyTCBDB *self, PyObject *args, PyObject *keywds) {
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

static PyObject *
PyTCBDB_rangefwm(PyTCBDB *self, PyObject *args, PyObject *keywds) {
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

TCXDB_Contains(PyTCBDB_Contains,PyTCBDB,tcbdbvsiz,bdb)
TCXDB___contains__(PyTCBDB___contains__,PyTCBDB,PyTCBDB_Contains)
TCXDB___getitem__(PyTCBDB___getitem__,PyTCBDB,tcbdbget,bdb,raise_tcbdb_error)

static PyObject *
PyTCBDB_curnew(PyTCBDB *self) {
  PyBDBCUR *cur;
  PyObject *args;

  args = Py_BuildValue("(O)", self);
  cur = (PyBDBCUR *)PyBDBCUR_new(&PyBDBCUR_Type, args, NULL);
  Py_DECREF(args);

  if (cur) {
    return (PyObject *)cur;
  }
  raise_tcbdb_error(self->bdb);
  return NULL;
}

static PyObject *
PyTCBDB_GetIter(PyTCBDB *self, itertype itype) {
  PyBDBCUR *cur;
  if ((cur = (PyBDBCUR *)PyTCBDB_curnew(self))) {
    cur->itype = itype;
    if (PyBDBCUR_first(cur)) {
      return (PyObject *)cur;
    }
    PyBDBCUR_dealloc(cur);
  }
  return NULL;
}

TCXDB_iters(PyTCBDB,PyTCBDB_GetIter_keys,PyTCBDB_GetIter_values,PyTCBDB_GetIter_items,PyTCBDB_GetIter)

TCXDB_rnum(TCBDB_rnum,TCBDB,tcbdbrnum)

static PyObject *
PyTCBDB_keys(PyTCBDB *self) {
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
    _key = PyString_FromStringAndSize(key, key_len);
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

static PyObject *
PyTCBDB_items(PyTCBDB *self) {
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

static PyObject *
PyTCBDB_values(PyTCBDB *self) {
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
    _value = PyString_FromStringAndSize(value, value_len);
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

TCXDB_length(PyTCBDB_length,PyTCBDB,TCBDB_rnum,bdb)
TCXDB_subscript(PyTCBDB_subscript,PyTCBDB,tcbdbget,bdb,raise_tcbdb_error)
TCXDB_DelItem(PyTCBDB_DelItem,PyTCBDB,tcbdbout,bdb,raise_tcbdb_error)
TCXDB_SetItem(PyTCBDB_SetItem,PyTCBDB,tcbdbput,bdb,raise_tcbdb_error)
TCXDB_ass_sub(PyTCBDB_ass_sub,PyTCBDB,PyTCBDB_SetItem,PyTCBDB_DelItem)

static PyMethodDef PyTCBDB_methods[] = {
  {"errmsg", (PyCFunction)PyTCBDB_errmsg,
   METH_VARARGS | METH_KEYWORDS | METH_CLASS,
   "Get the message string corresponding to an error code."},
  {"ecode", (PyCFunction)PyTCBDB_ecode,
   METH_NOARGS,
   "Get the last happened error code of a B+ tree database object."},
  {"setcmpfunc", (PyCFunction)PyTCBDB_setcmpfunc,
   METH_VARARGS | METH_KEYWORDS,
   "Set the custom comparison function of a B+ tree database object."},
  {"setmutex", (PyCFunction)PyTCBDB_setmutex,
   METH_NOARGS,
   "Set mutual exclusion control of a B+ tree database object for threading."},
  {"tune", (PyCFunction)PyTCBDB_tune,
   METH_VARARGS | METH_KEYWORDS,
   "Set the tuning parameters of a B+ tree database object."},
  {"setcache", (PyCFunction)PyTCBDB_setcache,
   METH_VARARGS | METH_KEYWORDS,
   "Set the caching parameters of a B+ tree database object."},
  {"open", (PyCFunction)PyTCBDB_open,
   METH_VARARGS | METH_KEYWORDS,
   "Open a database file and connect a B+ tree database object."},
  {"close", (PyCFunction)PyTCBDB_close,
   METH_NOARGS,
   "Close a B+ tree database object."},
  {"put", (PyCFunction)PyTCBDB_put,
   METH_VARARGS | METH_KEYWORDS,
   "Store a record into a B+ tree database object."},
  {"putkeep", (PyCFunction)PyTCBDB_putkeep,
   METH_VARARGS | METH_KEYWORDS,
   "Store a new record into a B+ tree database object."},
  {"putcat", (PyCFunction)PyTCBDB_putcat,
   METH_VARARGS | METH_KEYWORDS,
   "Concatenate a value at the end of the existing record in a B+ tree database object."},
  {"putdup", (PyCFunction)PyTCBDB_putdup,
   METH_VARARGS | METH_KEYWORDS,
   "Store a record into a B+ tree database object with allowing duplication of keys."},
  {"putlist", (PyCFunction)PyTCBDB_putlist,
   METH_VARARGS | METH_KEYWORDS,
   "Store records into a B+ tree database object with allowing duplication of keys."},
  {"out", (PyCFunction)PyTCBDB_out,
   METH_VARARGS | METH_KEYWORDS,
   "Remove a record of a B+ tree database object."},
  {"outlist", (PyCFunction)PyTCBDB_outlist,
   METH_VARARGS | METH_KEYWORDS,
   "Remove records of a B+ tree database object."},
  {"get", (PyCFunction)PyTCBDB_get,
   METH_VARARGS | METH_KEYWORDS,
   "Retrieve a record in a B+ tree database object.\n"
   "If the key of duplicated records is specified, the value of the first record is selected."},
  {"getlist", (PyCFunction)PyTCBDB_getlist,
   METH_VARARGS | METH_KEYWORDS,
   "Retrieve records in a B+ tree database object."},
  {"vnum", (PyCFunction)PyTCBDB_vnum,
   METH_VARARGS | METH_KEYWORDS,
   "Get the number of records corresponding a key in a B+ tree database object."},
  {"vsiz", (PyCFunction)PyTCBDB_vsiz,
   METH_VARARGS | METH_KEYWORDS,
   "Get the size of the value of a record in a B+ tree database object."},
  {"sync", (PyCFunction)PyTCBDB_sync,
   METH_NOARGS,
   "Synchronize updated contents of a B+ tree database object with the file and the device."},
  {"optimize", (PyCFunction)PyTCBDB_optimize,
   METH_VARARGS | METH_KEYWORDS,
   "Optimize the file of a B+ tree database object."},
  {"vanish", (PyCFunction)PyTCBDB_vanish,
   METH_NOARGS,
   "Remove all records of a hash database object."},
  {"copy", (PyCFunction)PyTCBDB_copy,
   METH_VARARGS | METH_KEYWORDS,
   "Copy the database file of a B+ tree database object."},
  {"tranbegin", (PyCFunction)PyTCBDB_tranbegin,
   METH_NOARGS,
   "Begin the transaction of a B+ tree database object."},
  {"trancommit", (PyCFunction)PyTCBDB_trancommit,
   METH_NOARGS,
   "Commit the transaction of a B+ tree database object."},
  {"tranabort", (PyCFunction)PyTCBDB_tranabort,
   METH_NOARGS,
   "Abort the transaction of a B+ tree database object."},
  {"path", (PyCFunction)PyTCBDB_path,
   METH_NOARGS,
   "Get the file path of a hash database object."},
  {"rnum", (PyCFunction)PyTCBDB_rnum,
   METH_NOARGS,
   "Get the number of records of a B+ tree database object."},
  {"fsiz", (PyCFunction)PyTCBDB_fsiz,
   METH_NOARGS,
   "Get the size of the database file of a B+ tree database object."},
  {"curnew", (PyCFunction)PyTCBDB_curnew,
   METH_NOARGS,
   "Create a cursor object."},
  {"range", (PyCFunction)PyTCBDB_range,
   METH_VARARGS | METH_KEYWORDS,
   ""},
  {"rangefwm", (PyCFunction)PyTCBDB_rangefwm,
   METH_VARARGS | METH_KEYWORDS,
   ""},
  {"__contains__", (PyCFunction)PyTCBDB___contains__,
   METH_O | METH_COEXIST,
   ""},
  {"__getitem__", (PyCFunction)PyTCBDB___getitem__,
   METH_O | METH_COEXIST,
   ""},
  {"has_key", (PyCFunction)PyTCBDB___contains__,
   METH_O,
   ""},
  {"keys", (PyCFunction)PyTCBDB_keys,
   METH_NOARGS,
   ""},
  {"items", (PyCFunction)PyTCBDB_items,
   METH_NOARGS,
   ""},
  {"values", (PyCFunction)PyTCBDB_values,
   METH_NOARGS,
   ""},
  {"iteritems", (PyCFunction)PyTCBDB_GetIter_items,
   METH_NOARGS,
   ""},
  {"iterkeys", (PyCFunction)PyTCBDB_GetIter_keys,
   METH_NOARGS,
   ""},
  {"itervalues", (PyCFunction)PyTCBDB_GetIter_values,
   METH_NOARGS,
   ""},
  {"addint", (PyCFunction)PyTCBDB_addint,
   METH_VARARGS | METH_KEYWORDS,
   "Add an integer to a record in a B+ tree database object."},
  {"adddouble", (PyCFunction)PyTCBDB_adddouble,
   METH_VARARGS | METH_KEYWORDS,
   "Add a real number to a record in a B+ tree database object."},
  /*
  {"", (PyCFunction)PyTCBDB_,
   METH_VARARGS | METH_KEYWORDS,
   ""},
  */
  {NULL, NULL, 0, NULL}
};

/* Hack to implement "key in dict" */
static PySequenceMethods PyTCBDB_as_sequence = {
  0,                             /* sq_length */
  0,                             /* sq_concat */
  0,                             /* sq_repeat */
  0,                             /* sq_item */
  0,                             /* sq_slice */
  0,                             /* sq_ass_item */
  0,                             /* sq_ass_slice */
  (objobjproc)PyTCBDB_Contains,  /* sq_contains */
  0,                             /* sq_inplace_concat */
  0,                             /* sq_inplace_repeat */
};

static PyMappingMethods PyTCBDB_as_mapping = {
  (lenfunc)PyTCBDB_length, /* mp_length */
  (binaryfunc)PyTCBDB_subscript, /* mp_subscript */
  (objobjargproc)PyTCBDB_ass_sub, /* mp_ass_subscript */
};

/* type objects */

static PyTypeObject PyTCBDB_Type = {
  PyObject_HEAD_INIT(NULL)
  0,                                           /* ob_size */
  "pytc.BDB",                                  /* tp_name */
  sizeof(PyTCBDB),                             /* tp_basicsize */
  0,                                           /* tp_itemsize */
  (destructor)PyTCBDB_dealloc,                 /* tp_dealloc */
  0,                                           /* tp_print */
  0,                                           /* tp_getattr */
  0,                                           /* tp_setattr */
  0,                                           /* tp_compare */
  0,                                           /* tp_repr */
  0,                                           /* tp_as_number */
  &PyTCBDB_as_sequence,                        /* tp_as_sequence */
  &PyTCBDB_as_mapping,                         /* tp_as_mapping */
  PyTCBDB_Hash,                                /* tp_hash  */
  0,                                           /* tp_call */
  0,                                           /* tp_str */
  0,                                           /* tp_getattro */
  0,                                           /* tp_setattro */
  0,                                           /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
  "tcbdb",                                     /* tp_doc */
  0,                                           /* tp_traverse */
  0,                                           /* tp_clear */
  0,                                           /* tp_richcompare */
  0,                                           /* tp_weaklistoffset */
  (getiterfunc)PyTCBDB_GetIter_keys,           /* tp_iter */
  0,                                           /* tp_iternext */
  PyTCBDB_methods,                             /* tp_methods */
  0,                                           /* tp_members */
  0,                                           /* tp_getset */
  0,                                           /* tp_base */
  0,                                           /* tp_dict */
  0,                                           /* tp_descr_get */
  0,                                           /* tp_descr_set */
  0,                                           /* tp_dictoffset */
  0,                                           /* tp_init */
  0,                                           /* tp_alloc */
  PyTCBDB_new,                                 /* tp_new */
};

#define ADD_INT(module,NAME) PyModule_AddIntConstant(module, #NAME, NAME)

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initpytc(void)
{
  PyObject *m, *mod_dic;

  if (!(m = Py_InitModule3("pytc", NULL,
                           "TokyoCabinet python bindings."))) {
    goto exit;
  }

  /* register classes */
  if (!(mod_dic = PyModule_GetDict(m))) { goto exit; }

  if (PyType_Ready(&PyTCHDB_Type) < 0) { goto exit; }
  if (PyType_Ready(&PyTCBDB_Type) < 0) { goto exit; }
  if (PyType_Ready(&PyBDBCUR_Type) < 0) { goto exit; }

  PyTCError = PyErr_NewException("pytc.Error", NULL, NULL);
  PyDict_SetItemString(mod_dic, "Error", PyTCError);

  Py_INCREF(&PyTCHDB_Type);
  PyModule_AddObject(m, "HDB", (PyObject *)&PyTCHDB_Type);
  Py_INCREF(&PyTCBDB_Type);
  PyModule_AddObject(m, "BDB", (PyObject *)&PyTCBDB_Type);
  Py_INCREF(&PyBDBCUR_Type);
  PyModule_AddObject(m, "BDBCUR", (PyObject *)&PyBDBCUR_Type);

  /* register consts */

  /* error */
  ADD_INT(m, TCESUCCESS);
  ADD_INT(m, TCETHREAD);
  ADD_INT(m, TCEINVALID);
  ADD_INT(m, TCENOFILE);
  ADD_INT(m, TCENOPERM);
  ADD_INT(m, TCEMETA);
  ADD_INT(m, TCERHEAD);
  ADD_INT(m, TCEOPEN);
  ADD_INT(m, TCECLOSE);
  ADD_INT(m, TCETRUNC);
  ADD_INT(m, TCESYNC);
  ADD_INT(m, TCESTAT);
  ADD_INT(m, TCESEEK);
  ADD_INT(m, TCEREAD);
  ADD_INT(m, TCEWRITE);
  ADD_INT(m, TCEMMAP);
  ADD_INT(m, TCELOCK);
  ADD_INT(m, TCEUNLINK);
  ADD_INT(m, TCERENAME);
  ADD_INT(m, TCEMKDIR);
  ADD_INT(m, TCERMDIR);
  ADD_INT(m, TCEKEEP);
  ADD_INT(m, TCENOREC);
  ADD_INT(m, TCEMISC);

  /* HDB */
  ADD_INT(m, HDBFOPEN);
  ADD_INT(m, HDBFFATAL);

  ADD_INT(m, HDBTLARGE);
  ADD_INT(m, HDBTDEFLATE);
  ADD_INT(m, HDBTBZIP);
  ADD_INT(m, HDBTTCBS);
  ADD_INT(m, HDBTEXCODEC);

  ADD_INT(m, HDBOREADER);
  ADD_INT(m, HDBOWRITER);
  ADD_INT(m, HDBOCREAT);
  ADD_INT(m, HDBOTRUNC);
  ADD_INT(m, HDBONOLCK);
  ADD_INT(m, HDBOLCKNB);

  /* BDB */
  ADD_INT(m, BDBFOPEN);
  ADD_INT(m, BDBFFATAL);

  ADD_INT(m, BDBTLARGE);
  ADD_INT(m, BDBTDEFLATE);
  ADD_INT(m, BDBTBZIP);
  ADD_INT(m, BDBTTCBS);
  ADD_INT(m, BDBTEXCODEC);

  ADD_INT(m, BDBOREADER);
  ADD_INT(m, BDBOWRITER);
  ADD_INT(m, BDBOCREAT);
  ADD_INT(m, BDBOTRUNC);
  ADD_INT(m, BDBONOLCK);
  ADD_INT(m, BDBOLCKNB);

  ADD_INT(m, BDBCPCURRENT);
  ADD_INT(m, BDBCPBEFORE);
  ADD_INT(m, BDBCPAFTER);

exit:
  if (PyErr_Occurred()) {
    /* PyErr_Print(); */
    PyErr_SetString(PyExc_ImportError, "pytc: init failed");
  }
}
