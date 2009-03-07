#include "FDB.h"

typedef TcfdbObject tcfdbobject;

static int
tcfdb_init(tcfdbobject *pyself, PyObject *args, PyObject *kwds)
{
    TcfdbObject *tcfdb = (TcfdbObject *)pyself;
    if ((tcfdb->db = tcfdbnew())) {
        int omode = 0;
        char *path = NULL;
        static char *kwlist[] = {"path", "omode", NULL};

        if (!PyArg_ParseTupleAndKeywords(args, kwds, "|si:open", kwlist, &path, &omode))
            return -1;

        if (path && omode) {
            bool result;
            Py_BEGIN_ALLOW_THREADS
            result = tcfdbopen(tcfdb->db, path, omode);
            Py_END_ALLOW_THREADS

            return result;
        }

        return 0;
    } else {
        PyErr_SetString(PyExc_MemoryError, "Cannot alloc tcfdb instance");
        return -1;
    }
}


static void
tcfdb_dealloc(tcfdbobject *self)
{
    Py_BEGIN_ALLOW_THREADS
    if (self) {
        tcfdbdel(self->db);
        self->db = 0;
    }
    Py_END_ALLOW_THREADS
}


static long
tcfdb_nohash(PyObject *pyself)
{
        PyErr_SetString(PyExc_TypeError, "tcbdb objects are unhashable");
        return -1;
}


static PyObject*
tcfdb_out(PyObject* pyself, PyObject* _key)
{
    TcfdbObject* self = (TcfdbObject*)pyself;
    assert(self->db);

    PY_LONG_LONG key = PyLong_AsLongLong(_key);
    if (PyErr_ExceptionMatches(PyExc_TypeError)) {
        PyErr_SetString(PyExc_TypeError, "Argument key is of wrong type");
        return NULL;
    }

    bool retval = false;

    Py_BEGIN_ALLOW_THREADS
    retval = tcfdbout(self->db, key);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(retval);
}

static PyObject*
tcfdb_get_impl(PyObject* pyself, PyObject* _key)
{
    TcfdbObject* self = (TcfdbObject*)pyself;
    assert(self->db);

    PY_LONG_LONG key = PyLong_AsLongLong(_key);
    if (PyErr_ExceptionMatches(PyExc_TypeError)) {
        PyErr_SetString(PyExc_TypeError, "Argument key is of wrong type");
        return NULL;
    }

    PyObject *retval = NULL;
    char *value = NULL;
    int value_len = 0;

    Py_BEGIN_ALLOW_THREADS
    value = tcfdbget(self->db, key, &value_len);
    Py_END_ALLOW_THREADS

    if (value) {
        retval = PyBytes_FromStringAndSize(value, value_len);
        free(value);
    }
    return retval;
}

static PyObject*
tcfdb_get(PyObject* pyself, PyObject* key)
{
    PyObject* retval = tcfdb_get_impl(pyself, key);

    if (!retval) {
        Py_INCREF(Py_None);
        retval = Py_None;
    }

    return retval;
}

static PyObject*
tcfdb___getitem__(PyObject *pyself, PyObject *key)
{
    PyObject* retval = tcfdb_get_impl(pyself, key);

    if (!retval) {
        PyErr_SetObject(PyExc_KeyError, key);
    }

    return retval;
}

enum StoreType
{
    PUT,
    PUTKEEP,
    PUTCAT
};

static PyObject*
tcfdb_put_impl(PyObject* pyself, PyObject* args, enum StoreType type)
{
    TcfdbObject* self = (TcfdbObject*)pyself;
    assert(self->db);

    PY_LONG_LONG key;
    char *value = NULL;
    int value_len = 0;
    if (!PyArg_ParseTuple(args, "Ls#:put", &key, &value, &value_len))
        return NULL;

    int retval = 0;

    Py_BEGIN_ALLOW_THREADS
    switch(type) {
        case PUT:
            retval = tcfdbput(self->db, key, value, value_len);
            break;
        case PUTKEEP:
            retval = tcfdbputkeep(self->db, key, value, value_len);
            break;
        case PUTCAT:
            retval = tcfdbputcat(self->db, key, value, value_len);
            break;
    }
    Py_END_ALLOW_THREADS

    return PyInt_FromLong(retval == 0 || tcfdbecode(self->db) == TCEKEEP);
}

static PyObject*
tcfdb_put(PyObject* pyself, PyObject* args)
{
    return tcfdb_put_impl(pyself, args, PUT);
}

static PyObject*
tcfdb_putkeep(PyObject* pyself, PyObject* args)
{
    return tcfdb_put_impl(pyself, args, PUTKEEP);
}

static PyObject*
tcfdb_putcat(PyObject* pyself, PyObject* args)
{
    return tcfdb_put_impl(pyself, args, PUTCAT);
}

static Py_ssize_t
tcfdb_length(tcfdbobject *self)
{
    Py_ssize_t retval;

    Py_BEGIN_ALLOW_THREADS
    retval = (Py_ssize_t)tcfdbrnum(self->db);
    Py_END_ALLOW_THREADS

    return retval;
}

static int
tcfdb___contains___impl(PyObject *pyself, PyObject *_key)
{
    TcfdbObject* self = (TcfdbObject*)pyself;
    assert(self->db);

    PY_LONG_LONG key = PyLong_AsLongLong(_key);
    if (PyErr_ExceptionMatches(PyExc_TypeError))
        return -1;

    int value_len = -1;

    Py_BEGIN_ALLOW_THREADS
    value_len = tcfdbvsiz(self->db, key);
    Py_END_ALLOW_THREADS

    return (value_len != -1);
}

static PyObject *
tcfdb___contains__(PyObject *pyself, PyObject *key)
{
    int retval = tcfdb___contains___impl(pyself, key);

    if (retval == -1)
        return NULL;

    return PyBool_FromLong(retval != 0);
}

static PyMethodDef tcfdb_methods[] = {
    {
        "__getitem__",  (PyCFunction)tcfdb___getitem__,     METH_O | METH_COEXIST,
        "get(key) -- Retrieves a key from the memcache.\n\n@return: The value or None."
    },
    {
        "__contains__", (PyCFunction)tcfdb___contains__,    METH_O | METH_COEXIST,
        NULL
    },
    {
        "has_key",      (PyCFunction)tcfdb___contains__,    METH_O,
        NULL},
    {
        "get",          (PyCFunction)tcfdb_get,             METH_O,
        "get(key) -- Retrieves a key from the database.\n\n@return: The value or None."
    },
    {
        "out",          (PyCFunction)tcfdb_out,             METH_O,
        "out(key) -- Deletes a key from the database.\n\n@return: True or False."
    },
    {
        "put",          (PyCFunction)tcfdb_put,             METH_VARARGS,
        "put(key, value) -- Unconditionally sets a key to a given value in the database.\n\n"
        "@return: Nonzero on success.\n@rtype: int\n"
    },
    {
        "putkeep",      (PyCFunction)tcfdb_putkeep,         METH_VARARGS,
        "putkeep(key, value) -- Unconditionally sets a key to a given value in the database.\n\n"
        "@return: Nonzero on success.\n@rtype: int\n"
    },
    {
        "putcat",       (PyCFunction)tcfdb_putcat,          METH_VARARGS,
        "putcat(key, value) -- Unconditionally sets a key to a given value in the database.\n\n"
        "@return: Nonzero on success.\n@rtype: int\n"
    },
    {NULL}  /* Sentinel */
};

static PyMappingMethods tcfdb_as_mapping = {
        (lenfunc)tcfdb_length,                  /*mp_length*/
        (binaryfunc)tcfdb___getitem__,          /*mp_subscript*/
        0, /* (objobjargproc)dict_ass_sub,      /*mp_ass_subscript*/
};

/* Hack to implement "key in dict" */
static PySequenceMethods tcfdb_as_sequence = {
  0,                                            /* sq_length */
  0,                                            /* sq_concat */
  0,                                            /* sq_repeat */
  0,                                            /* sq_item */
  0,                                            /* sq_slice */
  0,                                            /* sq_ass_item */
  0,                                            /* sq_ass_slice */
  (objobjproc)tcfdb___contains___impl,          /* sq_contains */
  0,                                            /* sq_inplace_concat */
  0,                                            /* sq_inplace_repeat */
};

PyTypeObject PyTcFdb_Type = {
    #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    #else
    PyVarObject_HEAD_INIT(NULL, 0)
    #endif
    "tc.FDB",                                   /* tp_name */
    sizeof(tcfdbobject),                        /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)tcfdb_dealloc,                  /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    &tcfdb_as_sequence,                         /* tp_as_sequence */
    &tcfdb_as_mapping,                          /* tp_as_mapping */
    tcfdb_nohash,                               /* tp_hash  */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    "Tokyo Cabinet fixed-width database",       /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    tcfdb_methods,                              /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)tcfdb_init,                       /* tp_init */
    0,                                          /* tp_alloc */
    PyType_GenericNew,                          /* tp_new */
};


int tcfdb_register(PyObject *module) {
    if (PyType_Ready(&PyTcFdb_Type) == 0)
        return PyModule_AddObject(module, "FDB", (PyObject *)&PyTcFdb_Type);

    return -1;
}