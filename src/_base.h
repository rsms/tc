#ifndef PYTC__BASE_H
#define PYTC__BASE_H

#include "_func_macros.h"

#include <Python.h>
#include <pyconfig.h>
#include <structmember.h>

#include <stdint.h>

#include <tcutil.h>

/* Types */
#if (PY_VERSION_HEX < 0x02050000)
  typedef inquiry lenfunc;
  typedef int     Py_ssize_t;
  #define PY_SSIZE_FMT "%d"
#else
  #define PY_SSIZE_FMT "%zd"
#endif
typedef uint8_t byte;
typedef enum {
  tc_iter_key_t = 0,
  tc_iter_value_t,
  tc_iter_item_t
} tc_itertype_t;

/* Convert an ASCII hex digit to the corresponding number between 0
   and 15.  H should be a hexadecimal digit that satisfies isxdigit;
   otherwise, the result is undefined.  */
#define XDIGIT_TO_NUM(h) ((h) < 'A' ? (h) - '0' : toupper(h) - 'A' + 10)
#define X2DIGITS_TO_NUM(h1, h2) ((XDIGIT_TO_NUM (h1) << 4) + XDIGIT_TO_NUM (h2))

/* The reverse of the above: convert a number in the [0, 16) range to
   the ASCII representation of the corresponding hexadecimal digit.
   `+ 0' is there so you can't accidentally use it as an lvalue.  */
#define XNUM_TO_DIGIT(x) ("0123456789ABCDEF"[x] + 0)
#define XNUM_TO_digit(x) ("0123456789abcdef"[x] + 0)


/* Python 3 is on the horizon... (currently only used by ) */
#if (PY_VERSION_HEX < 0x02060000)  /* really: before python trunk r63675 */
  /* These #defines map to their equivalent on earlier python versions. */
  #define PyBytes_FromStringAndSize   PyString_FromStringAndSize
  #define PyBytes_FromString          PyString_FromString
  #define PyBytes_AsStringAndSize     PyString_AsStringAndSize
  #define PyBytes_Check               PyString_Check
  #define PyBytes_GET_SIZE            PyString_GET_SIZE
  #define PyBytes_AS_STRING           PyString_AS_STRING
  #define PyBytes_AsString            PyString_AsString
  #define PyBytes_FromFormat          PyString_FromFormat
  #define PyBytes_Size                PyString_Size
  #define PyBytes_ConcatAndDel        PyString_ConcatAndDel
  #define PyBytes_Concat              PyString_Concat
  #define _PyBytes_Resize             _PyString_Resize
#endif
#if (PY_VERSION_HEX >= 0x03000000)
  #define NUMBER_Check    PyLong_Check
  #define NUMBER_AsLong   PyLong_AsLong
  #define NUMBER_FromLong PyLong_FromLong
#else
  #define NUMBER_Check    PyInt_Check
  #define NUMBER_AsLong   PyInt_AsLong
  #define NUMBER_FromLong PyInt_FromLong
#endif


/* Get minimum value */
#ifndef min
  #define min(X, Y)  ((X) < (Y) ? (X) : (Y))
#endif
#ifndef max
  #define max(X, Y)  ((X) > (Y) ? (X) : (Y))
#endif

/* Module identifier, used in logging */
#define MOD_IDENT "tc"

/* Converting MY_MACRO to "<value of MY_MACRO>" i.e. QUOTE(PY_MAJOR_VERSION) -> "2" */
#define _QUOTE(x) #x
#define QUOTE(x) _QUOTE(x)

/* Replace a PyObject while counting references */
#define REPLACE_OBJ(destination, new_value, type) \
  do { \
    type *__old_ ## type ## __LINE__ = (type *)(destination); \
    (destination) = (type *)(new_value); \
    Py_XINCREF(destination); \
    Py_XDECREF(__old_ ## type ## __LINE__); \
  } while (0)

#define REPLACE_PyObject(dst, value) REPLACE_OBJ(dst, value, PyObject)

#ifndef Py_TYPE
  #define Py_TYPE(ob)	(((PyObject*)(ob))->ob_type)
#endif

/* Ensure a lazy instance variable is available */
#define ENSURE_BY_GETTER(direct, getter, ...) \
  if (direct == NULL) {\
    PyObject *tmp = getter;\
    if (tmp == NULL) {\
      __VA_ARGS__ ;\
    } else {\
      Py_DECREF(tmp);\
    }\
  }

/* Get and set arbitrary attributes of objects.
 * This is the only way to set/get Type/Class properties.
 * Uses the interal function PyObject **_PyObject_GetDictPtr(PyObject *);
 * Returns PyObject * (NULL if an exception was raised)
 */
#define PYTC_PyObject_GET(PyObject_ptr_type, char_ptr_name) \
  PyDict_GetItemString(*_PyObject_GetDictPtr((PyObject *)PyObject_ptr_type), char_ptr_name)

/* Returns 0 on success, -1 on failure. */
#define PYTC_PyObject_SET(PyObject_ptr_type, char_ptr_name, PyObject_ptr_value) \
  PyDict_SetItemString(*_PyObject_GetDictPtr((PyObject *)PyObject_ptr_type), char_ptr_name,\
  (PyObject *)PyObject_ptr_value)

/* instanceof(obj, (bytes, str, unicode) */
#define PYTC_STRING_CHECK(obj) (PyBytes_Check(obj) || PyUnicode_Check(obj))

/* Set IOError with errno and filename info. Return NULL */
#define PyErr_SET_FROM_ERRNO   PyErr_SetFromErrnoWithFilename(PyExc_IOError, __FILE__)

/* Log to stderr */
#define log_error(fmt, ...) fprintf(stderr, MOD_IDENT " [%d] ERROR %s:%d: " fmt "\n", \
  getpid(), __FILE__, __LINE__, ##__VA_ARGS__)

/* Used for temporary debugging */
#define _DUMP_REFCOUNT(o) log_error("*** %s: %ld", #o, (o) ? (long int)(o)->ob_refcnt : 0)

/* Log to stderr, but only in debug builds */
#if DEBUG
  #define log_debug(fmt, ...) fprintf(stderr, MOD_IDENT " [%d] DEBUG %s:%d: " fmt "\n",\
    getpid(), __FILE__, __LINE__, ##__VA_ARGS__)
  #define IFDEBUG(x) x
  #define assert_refcount(o, count_test) \
    do { \
      if (!((o)->ob_refcnt count_test)){ \
        log_debug("assert_refcount(" PY_SSIZE_FMT ", %s)", (Py_ssize_t)(o)->ob_refcnt, #count_test);\
      }\
      assert((o)->ob_refcnt count_test); \
    } while (0)
  #define DUMP_REFCOUNT(o) log_debug("*** %s: " PY_SSIZE_FMT, #o, (o) ? (Py_ssize_t)(o)->ob_refcnt : 0)
  #define DUMP_REPR(o) \
    do { PyObject *repr = PyObject_Repr((PyObject *)(o));\
      if (repr) {\
        log_debug("repr(%s) = %s", #o, PyBytes_AS_STRING(repr));\
        Py_DECREF(repr);\
      } else {\
        log_debug("repr(%s) = <NULL>", #o);\
      }\
    } while (0)
#else
  #define log_debug(fmt, ...) ((void)0)
  #define assert_refcount(o, count_test) 
  #define IFDEBUG(x) 
  #define DUMP_REFCOUNT(o) 
  #define DUMP_REPR(o) 
#endif

#if PYTC_TRACE
  #define log_trace(fmt, ...) fprintf(stderr, MOD_IDENT " [%d] TRACE %s:%d in %s " fmt "\n", \
    getpid(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
  #define IFTRACE(x) x
#else
  #define log_trace(fmt, ...) ((void)0)
  #define IFTRACE(x)
#endif

#endif
