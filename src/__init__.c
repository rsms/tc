#include "__init__.h"
#include <tcadb.h>
#include <tcbdb.h>
#include <tcfdb.h>
#include <tchdb.h>
#include <tctdb.h>

#include "HDB.h"
#include "BDB.h"
#include "BDBCursor.h"

PyObject *tc_module;
PyObject *tc_Error;


/*
 * Module functions
 */
static PyMethodDef tc_functions[] = {
  {NULL, NULL}
};


/*
 * Module structure (Only used in Python >=3.0)
 */
#if (PY_VERSION_HEX >= 0x03000000)
  static struct PyModuleDef tc_module_t = {
    PyModuleDef_HEAD_INIT,
    "_tc",   /* Name of module */
    NULL,    /* module documentation, may be NULL */
    -1,      /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
    tc_functions,
    NULL,   /* Reload */
    NULL,   /* Traverse */
    NULL,   /* Clear */
    NULL    /* Free */
  };
#endif


/*
 * Module initialization
 */
#if (PY_VERSION_HEX < 0x03000000)
DL_EXPORT(void) init_tc(void)
#else
PyMODINIT_FUNC  PyInit__tc(void)
#endif
{
  /* Create module */
  #if (PY_VERSION_HEX < 0x03000000)
    tc_module = Py_InitModule("_tc", tc_functions);
  #else
    tc_module = PyModule_Create(&tc_module_t);
  #endif
  if (tc_module == NULL)
    goto exit;
  
  /* Create exceptions */
  tc_Error = PyErr_NewException("tc.Error", NULL, NULL);
  if ( (tc_Error == NULL) || (PyModule_AddObject(tc_module, "Error", tc_Error) == -1) )
    goto exit;
  
  /* Register types */
  #define R(name, okstmt) \
    if (name(tc_module) okstmt) { \
      log_error("sub-component initializer '" #name "' failed"); \
      goto exit; \
    }
  R(tc_HDB_register, != 0)
  R(tc_BDB_register, != 0)
  R(tc_BDBCursor_register, != 0)
  #undef R

  /* Register consts */
  #define ADD_INT(module, NAME) PyModule_AddIntConstant(module, #NAME, NAME)

  /* Errors (defined in tcutil.h) */
  ADD_INT(tc_module, TCESUCCESS);
  ADD_INT(tc_module, TCETHREAD);
  ADD_INT(tc_module, TCEINVALID);
  ADD_INT(tc_module, TCENOFILE);
  ADD_INT(tc_module, TCENOPERM);
  ADD_INT(tc_module, TCEMETA);
  ADD_INT(tc_module, TCERHEAD);
  ADD_INT(tc_module, TCEOPEN);
  ADD_INT(tc_module, TCECLOSE);
  ADD_INT(tc_module, TCETRUNC);
  ADD_INT(tc_module, TCESYNC);
  ADD_INT(tc_module, TCESTAT);
  ADD_INT(tc_module, TCESEEK);
  ADD_INT(tc_module, TCEREAD);
  ADD_INT(tc_module, TCEWRITE);
  ADD_INT(tc_module, TCEMMAP);
  ADD_INT(tc_module, TCELOCK);
  ADD_INT(tc_module, TCEUNLINK);
  ADD_INT(tc_module, TCERENAME);
  ADD_INT(tc_module, TCEMKDIR);
  ADD_INT(tc_module, TCERMDIR);
  ADD_INT(tc_module, TCEKEEP);
  ADD_INT(tc_module, TCENOREC);
  ADD_INT(tc_module, TCEMISC);

  /* HDB */
  ADD_INT(tc_module, HDBFOPEN);
  ADD_INT(tc_module, HDBFFATAL);

  ADD_INT(tc_module, HDBTLARGE);
  ADD_INT(tc_module, HDBTDEFLATE);
  ADD_INT(tc_module, HDBTBZIP);
  ADD_INT(tc_module, HDBTTCBS);
  ADD_INT(tc_module, HDBTEXCODEC);

  ADD_INT(tc_module, HDBOREADER);
  ADD_INT(tc_module, HDBOWRITER);
  ADD_INT(tc_module, HDBOCREAT);
  ADD_INT(tc_module, HDBOTRUNC);
  ADD_INT(tc_module, HDBONOLCK);
  ADD_INT(tc_module, HDBOLCKNB);

  /* BDB */
  ADD_INT(tc_module, BDBFOPEN);
  ADD_INT(tc_module, BDBFFATAL);

  ADD_INT(tc_module, BDBTLARGE);
  ADD_INT(tc_module, BDBTDEFLATE);
  ADD_INT(tc_module, BDBTBZIP);
  ADD_INT(tc_module, BDBTTCBS);
  ADD_INT(tc_module, BDBTEXCODEC);

  ADD_INT(tc_module, BDBOREADER);
  ADD_INT(tc_module, BDBOWRITER);
  ADD_INT(tc_module, BDBOCREAT);
  ADD_INT(tc_module, BDBOTRUNC);
  ADD_INT(tc_module, BDBONOLCK);
  ADD_INT(tc_module, BDBOLCKNB);

  ADD_INT(tc_module, BDBCPCURRENT);
  ADD_INT(tc_module, BDBCPBEFORE);
  ADD_INT(tc_module, BDBCPAFTER);
  
  #undef ADD_INT
  /* end adding constants */

exit:
  if (PyErr_Occurred()) {
    PyErr_Print();
    PyErr_SetString(PyExc_ImportError, "can't initialize module _tc");
    Py_XDECREF(tc_module);
    tc_module = NULL;
  }
  
  #if (PY_VERSION_HEX < 0x03000000)
    return;
  #else
    return tc_module;
  #endif
}
