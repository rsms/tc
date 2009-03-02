#ifndef PYTC__FUNC_MACROS_H
#define PYTC__FUNC_MACROS_H

#define TC_BOOL_NOARGS(func,type,call,member,err,errmember) \
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
#define TC_STRING_NOARGS(func,type,call,member,err) \
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
    ret = PyBytes_FromString(str); \
    return ret; \
  } \

/* NOTE: this function dealloc pointer returned by tc */
#define TC_STRINGL_NOARGS(func,type,call,member,err,errmember) \
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
    ret = PyBytes_FromStringAndSize(str, str_len); \
    free(str); \
    return ret; \
  } \

#define TC_NUM_NOARGS(func,type,rettype,call,member,ecode,err,conv) \
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

#define TC_U_LONG_LONG_NOARGS(func,type,call,member,ecode,err) \
  TC_NUM_NOARGS(func, type, unsigned PY_LONG_LONG, call, member, ecode, err, \
                PyLong_FromUnsignedLongLong)

#define TC_BOOL_KEYARGS(func,type,method,call,member,error,errmember) \
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

#define TC_INT_KEYARGS(func,type,method,call,member,error) \
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
    return NUMBER_FromLong((long)ret); \
  }

/* NOTE: this function dealloc pointer returned by tc */
#define TC_STRINGL_KEYARGS(func,type,method,call,member,error) \
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
    ret = PyBytes_FromStringAndSize(value, value_len); \
    free(value); \
    return ret; \
  }

#define TC_XDB_PUT(func,type,method,call,member,error) \
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

#define TC_XDB_OPEN(func,type,call_new,call_open,member,call_dealloc,error) \
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

#define TC_BOOL_PATHARGS(func,type,method,call,member,error) \
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
#define TC_XDB_rnum(func,type,call) \
  static uint64_t \
  func(type *obj) { \
    uint64_t ret; \
    Py_BEGIN_ALLOW_THREADS \
    ret = call(obj); \
    Py_END_ALLOW_THREADS \
    return ret; \
  }

/* for dict like interface */
#define TC_XDB_length(func,type,call,member) \
  static long \
  func(type *self) { \
    return call(self->member); \
  } \

#define TC_XDB_subscript(func,type,call,member,err) \
  static PyObject * \
  func(type *self, PyObject *_key) { \
    PyObject *ret; \
    char *key, *value; \
    int key_len, value_len; \
  \
    if (!PyBytes_Check(_key)) { \
      PyErr_SetString(PyExc_TypeError, "only string is allowed in []"); \
      return NULL; \
    } \
    key = PyBytes_AsString(_key); \
    key_len = PyBytes_GET_SIZE(_key); \
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
    ret = PyBytes_FromStringAndSize(value, value_len); \
    free(value); \
    return ret; \
  } \

#define TC_XDB_DelItem(func,type,call,member,err) \
  int \
  func(type *self, PyObject *_key) { \
    bool result; \
    char *key = PyBytes_AsString(_key); \
    int key_len = PyBytes_GET_SIZE(_key); \
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

#define TC_XDB_ass_sub(func,type,setitem,delitem) \
  static int \
  func(type *self, PyObject *v, PyObject *w) \
  { \
    if (w) { \
      return setitem(self, v, w); \
    } else { \
      return delitem(self, v); \
    } \
  }

#define TC_XDB_Contains(func,type,call,member) \
  static int \
  func(type *self, PyObject *_key) { \
    char *key = PyBytes_AsString(_key); \
    int key_len = PyBytes_GET_SIZE(_key), value_len; \
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

#define TC_XDB___contains__(func,type,call) \
  static PyObject * \
  func(type *self, PyObject *_key) { \
    return PyBool_FromLong(call(self, _key) != 0); \
  }

#define TC_XDB___getitem__(func,type,call,member,err) \
  static PyObject * \
  func(type *self, PyObject *_key) { \
    PyObject *ret; \
    char *key = PyBytes_AsString(_key), *value; \
    int key_len = PyBytes_GET_SIZE(_key), value_len; \
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
    ret = PyBytes_FromStringAndSize(value, value_len); \
    free(value); \
    return ret; \
  }

#define TC_XDB_iters(type,funcikey,funcival,funciitem,call) \
  static PyObject * \
  funcikey(type *self) { \
    return call(self, tc_iter_key_t); \
  } \
  \
  static PyObject * \
  funcival(type *self) { \
    return call(self, tc_iter_value_t); \
  } \
  \
  static PyObject * \
  funciitem(type *self) { \
    return call(self, tc_iter_item_t); \
  }

/* for TCXSTR key/value */
#define TC_GET_TCXSTR_KEY_VALUE(call,callarg) \
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

#define TC_CLEAR_TCXSTR_KEY_VALUE() \
  } \
  if (key) { tcxstrdel(key); } \
  if (value) { tcxstrdel(value); } \
  return ret;


/*** TC*DB ***/

#define TC_XDB_addint(func,type,method,call,member,err) \
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

#define TC_XDB_adddouble(func,type,method,call,member,err) \
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

#endif
