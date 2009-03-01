#include "util.h"
#include "__init__.h"

bool char_bounds(short x) {
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

void tc_Error_SetCodeAndString(int ecode, const char *errmsg) {
  PyObject *obj;
  obj = Py_BuildValue("(is)", ecode, errmsg);
  PyErr_SetObject(tc_Error, obj);
  Py_DECREF(obj);
}
