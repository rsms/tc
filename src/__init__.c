#include "__init__.h"
#include <tcadb.h>
#include <tcbdb.h>
#include <tcfdb.h>
#include <tchdb.h>
#include <tctdb.h>

#include "HDB.h"
#include "BDB.h"
#include "BDBCursor.h"
#include "TDB.h"
#include "TDBQuery.h"

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
  R(tc_TDB_register, != 0)
  R(tc_TDBQuery_register, != 0)
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
  ADD_INT(tc_module, HDBOTSYNC);

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
  ADD_INT(tc_module, BDBOTSYNC);

  ADD_INT(tc_module, BDBCPCURRENT);
  ADD_INT(tc_module, BDBCPBEFORE);
  ADD_INT(tc_module, BDBCPAFTER);
  /* end of BDB */
  
  /* TDB: additional flags */
  ADD_INT(tc_module, TDBFOPEN);     /* whether opened */
  ADD_INT(tc_module, TDBFFATAL);    /* whetehr with fatal error */
  
  /* TDB: tuning options */
  ADD_INT(tc_module, TDBTLARGE);    /* use 64-bit bucket array */
  ADD_INT(tc_module, TDBTDEFLATE);  /* compress each page with Deflate */
  ADD_INT(tc_module, TDBTBZIP);     /* compress each record with BZIP2 */
  ADD_INT(tc_module, TDBTTCBS);     /* compress each page with TCBS */
  ADD_INT(tc_module, TDBTEXCODEC);  /* compress each record with outer functions */
  
  /* TDB: open modes */
  ADD_INT(tc_module, TDBOREADER);   /* open as a reader */
  ADD_INT(tc_module, TDBOWRITER);   /* open as a writer */
  ADD_INT(tc_module, TDBOCREAT);    /* writer creating */
  ADD_INT(tc_module, TDBOTRUNC);    /* writer truncating */
  ADD_INT(tc_module, TDBONOLCK);    /* open without locking */
  ADD_INT(tc_module, TDBOLCKNB);    /* lock without blocking */
  ADD_INT(tc_module, TDBOTSYNC);    /* synchronize every transaction */
  
  /* TDB: index types */
  ADD_INT(tc_module, TDBITLEXICAL); /* lexical string */
  ADD_INT(tc_module, TDBITDECIMAL); /* decimal string */
  ADD_INT(tc_module, TDBITOPT);     /* optimize */
  ADD_INT(tc_module, TDBITVOID);    /* void */
  ADD_INT(tc_module, TDBITKEEP);    /* keep existing index */
  
  /* TDB: query conditions */
  ADD_INT(tc_module, TDBQCSTREQ);   /* string is equal to */
  ADD_INT(tc_module, TDBQCSTRINC);  /* string is included in */
  ADD_INT(tc_module, TDBQCSTRBW);   /* string begins with */
  ADD_INT(tc_module, TDBQCSTREW);   /* string ends with */
  ADD_INT(tc_module, TDBQCSTRAND);  /* string includes all tokens in */
  ADD_INT(tc_module, TDBQCSTROR);   /* string includes at least one token in */
  ADD_INT(tc_module, TDBQCSTROREQ); /* string is equal to at least one token in */
  ADD_INT(tc_module, TDBQCSTRRX);   /* string matches regular expressions of */
  ADD_INT(tc_module, TDBQCNUMEQ);   /* number is equal to */
  ADD_INT(tc_module, TDBQCNUMGT);   /* number is greater than */
  ADD_INT(tc_module, TDBQCNUMGE);   /* number is greater than or equal to */
  ADD_INT(tc_module, TDBQCNUMLT);   /* number is less than */
  ADD_INT(tc_module, TDBQCNUMLE);   /* number is less than or equal to */
  ADD_INT(tc_module, TDBQCNUMBT);   /* number is between two tokens of */
  ADD_INT(tc_module, TDBQCNUMOREQ); /* number is equal to at least one token in */
  ADD_INT(tc_module, TDBQCNEGATE);  /* negation flag */
  ADD_INT(tc_module, TDBQCNOIDX);   /* no index flag */
  
  /* TDB: order types */
  ADD_INT(tc_module, TDBQOSTRASC);  /* string ascending */
  ADD_INT(tc_module, TDBQOSTRDESC); /* string descending */
  ADD_INT(tc_module, TDBQONUMASC);  /* number ascending */
  ADD_INT(tc_module, TDBQONUMDESC); /* number descending */
  
  /* TDB: post treatments */
  ADD_INT(tc_module, TDBQPPUT);     /* modify the record */
  ADD_INT(tc_module, TDBQPOUT);     /* remove the record */
  ADD_INT(tc_module, TDBQPSTOP);    /* stop the iteration */
  
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
