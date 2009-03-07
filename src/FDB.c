#include "FDB.h"
#include "__init__.h" /* for tc_Error */

void
tc_fdb_seterr(TcFdbObject *self)
{
    int ecode = tcfdbecode(self->db);

    PyObject *obj;
    obj = Py_BuildValue("(is)", ecode, tcfdberrmsg(ecode));
    PyErr_SetObject(tc_Error, obj);
    Py_DECREF(obj);
}

static int
tc_fdb_init(PyObject *pyself, PyObject *args, PyObject *kwds)
{
    TcFdbObject *self = (TcFdbObject *)pyself;

    if ((self->db = tcfdbnew())) {
        int omode = 0;
        char *path = NULL;
        static char *kwlist[] = {"path", "omode", NULL};

        if (!PyArg_ParseTupleAndKeywords(args, kwds, "|si:open", kwlist, &path, &omode))
            return -1;

        if (path && omode) {
            bool result;
            Py_BEGIN_ALLOW_THREADS
            result = tcfdbopen(self->db, path, omode);
            Py_END_ALLOW_THREADS

            if (!result) {
                tc_fdb_seterr(self);
                return -1;
            }
        }

        return 0;
    } else {
        PyErr_SetString(PyExc_MemoryError, "Cannot alloc tcfdb instance");
        return -1;
    }
}

static void
tc_fdb_dealloc(TcFdbObject *self)
{
    Py_BEGIN_ALLOW_THREADS
    if (self->db) {
        tcfdbdel(self->db);
        self->db = NULL;
    }
    Py_END_ALLOW_THREADS
}

static long
tc_fdb_nohash(PyObject *pyself)
{
        PyErr_SetString(PyExc_TypeError, "tcbdb objects are unhashable");
        return -1;
}

static int
tc_fdb_out_impl(TcFdbObject* self, PyObject *pykey)
{
    PY_LONG_LONG key = PyLong_AsLongLong(pykey);
    if (PyErr_ExceptionMatches(PyExc_TypeError))
        return -1;

    bool retval = false;

    Py_BEGIN_ALLOW_THREADS
    retval = tcfdbout(self->db, key);
    Py_END_ALLOW_THREADS

    return (retval ? 0 : 1);
}

static PyObject *
tc_fdb_out(PyObject *pyself, PyObject *pykey)
{
    int retval = tc_fdb_out_impl((TcFdbObject *)pyself, pykey);
    if (retval < 0)
        return NULL;

    return PyBool_FromLong(!retval);
}

static PyObject *
tc_fdb_get_impl(TcFdbObject *self, PY_LONG_LONG key)
{
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

static PyObject *
tc_fdb_get_arg_impl(TcFdbObject *self, PyObject *pykey)
{
    PY_LONG_LONG key = PyLong_AsLongLong(pykey);
    if (PyErr_Occurred() != NULL && PyErr_ExceptionMatches(PyExc_TypeError))
        return NULL;

    return tc_fdb_get_impl(self, key);
}

static PyObject *
tc_fdb_get(PyObject *pyself, PyObject *pykey)
{
    TcFdbObject* self = (TcFdbObject *)pyself;
    assert(self->db);

    PyObject *retval = tc_fdb_get_arg_impl(self, pykey);

    if (retval == NULL) {
        if (PyErr_Occurred() != NULL)
            return NULL;

        Py_RETURN_NONE;
    }

    return retval;
}

static PyObject *
tc_fdb___getitem__impl(TcFdbObject *self, PyObject *pykey)
{
    PyObject *retval = tc_fdb_get_arg_impl(self, pykey);

    if (retval == NULL && PyErr_Occurred() == NULL)
        PyErr_SetObject(PyExc_KeyError, pykey);

    return retval;
}

static PyObject *
tc_fdb___getitem__(PyObject *pyself, PyObject *pykey)
{
    return tc_fdb___getitem__impl((TcFdbObject *)pyself, pykey);
}

static int
tc_fdb_put_impl(TcFdbObject* self, PyObject *pykey, PyObject *pyvalue, enum PutType type)
{
    PY_LONG_LONG key = PyLong_AsLongLong(pykey);
    if (PyErr_ExceptionMatches(PyExc_TypeError))
        return -1;

    char *value = PyString_AsString(pyvalue);
    int value_len = PyString_GET_SIZE(pyvalue);

    int retval = -1;
    bool ret;

    Py_BEGIN_ALLOW_THREADS
    switch(type) {
        case PUT_TYPE_DEFAULT:
            ret = tcfdbput(self->db, key, value, value_len);
            retval = (ret ? 0 : -1);
            break;
        case PUT_TYPE_KEEP:
            ret = tcfdbputkeep(self->db, key, value, value_len);
            retval = (!ret && tcfdbecode(self->db) == TCEKEEP) ? 1 : (ret ? 0 : -1);
            break;
        case PUT_TYPE_CAT:
            ret = tcfdbputcat(self->db, key, value, value_len);
            retval = (ret ? 0 : -1);
            break;
    }
    Py_END_ALLOW_THREADS

    if (retval < 0)
        tc_fdb_seterr(self);

    return retval;
}

static int
tc_fdb_put_args_impl(TcFdbObject *self, PyObject *args, enum PutType type)
{
    PyObject *key;
    PyObject *value;
    if (!PyArg_ParseTuple(args, "OO:put", &key, &value))
        return -1;

    return tc_fdb_put_impl(self, key, value, type);
}

static PyObject *
tc_fdb_put(PyObject *pyself, PyObject *args)
{
    int retval = tc_fdb_put_args_impl((TcFdbObject *)pyself, args, PUT_TYPE_DEFAULT);

    if (retval < 0)
        return NULL;

    return PyBool_FromLong(1);
}

static PyObject *
tc_fdb_putkeep(PyObject *pyself, PyObject *args)
{
    int retval = tc_fdb_put_args_impl((TcFdbObject *)pyself, args, PUT_TYPE_KEEP);

    if (retval < 0)
        return NULL;

    return PyBool_FromLong(!retval);
}

static PyObject *
tc_fdb_putcat(PyObject *pyself, PyObject *args)
{
    int retval = tc_fdb_put_args_impl((TcFdbObject *)pyself, args, PUT_TYPE_CAT);

    if (retval < 0)
        return NULL;

    return PyBool_FromLong(1);
}

static int
tc_fdb_ass_subscript(TcFdbObject *self, PyObject *key, PyObject *value)
{
    if (value == NULL) {
        int retval = tc_fdb_out_impl(self, key);
        if (retval != 0 && !PyErr_Occurred())
            PyErr_SetObject(PyExc_KeyError, key);

        return retval;
    } else {
        return tc_fdb_put_impl(self, key, value, PUT_TYPE_DEFAULT);
    }
}

static Py_ssize_t
tc_fdb_length(TcFdbObject *self)
{
    Py_ssize_t retval;

    Py_BEGIN_ALLOW_THREADS
    retval = (Py_ssize_t)tcfdbrnum(self->db);
    Py_END_ALLOW_THREADS

    return retval;
}

static int
tc_fdb___contains___impl(PyObject *pyself, PyObject *_key)
{
    TcFdbObject *self = (TcFdbObject *)pyself;
    assert(self->db);

    PY_LONG_LONG key = PyLong_AsLongLong(_key);
    if (PyErr_ExceptionMatches(PyExc_TypeError))
        return -1;

    int value_len = -1;

    Py_BEGIN_ALLOW_THREADS
    value_len = tcfdbvsiz(self->db, key);
    Py_END_ALLOW_THREADS

    return ((value_len == -1) ? 0 : 1);
}

static PyObject *
tc_fdb___contains__(PyObject *pyself, PyObject *key)
{
    int retval = tc_fdb___contains___impl(pyself, key);

    if (retval == -1)
        return NULL;

    return PyBool_FromLong(retval == 0);
}

static PyObject *
tc_fdb_list(TcFdbObject *self, enum IterType itertype)
{
    TcFdbIterObject *it = (TcFdbIterObject *)tc_fdb_iter_new(self, itertype);
    PyObject *retval = PyList_New(tc_fdb_iter_len_impl(it));
    if (it == NULL || retval == NULL)
        return NULL;

    PyObject *i;
    int index = 0;
    while((i = tc_fdb_iter_next(it))) {
        PyList_SET_ITEM(retval, index, i);
        index++;
    }

    return retval;
}

static PyObject *
tc_fdb_keys(PyObject *pyself)
{
    return tc_fdb_list((TcFdbObject *)pyself, FDB_ITER_TYPE_KEY);
}

static PyObject *
tc_fdb_values(PyObject *pyself)
{
    return tc_fdb_list((TcFdbObject *)pyself, FDB_ITER_TYPE_VALUE);
}

PyDoc_STRVAR(has_key__doc__,
"DB.has_key(k) -> True if DB has a key k, else False.");

PyDoc_STRVAR(__contains____doc__,
"DB.__contains__(k) -> True if DB has a key k, else False.");

PyDoc_STRVAR(__getitem____doc__,
"DB.__getitem__(k) <==> DB[k]");

PyDoc_STRVAR(__setitem____doc__,
"DB.__setitem__(k, v) <==> DB[k] = v");

PyDoc_STRVAR(get__doc__,
"DB.get(k) -> DB[k] if k in DB, else None.\n\nCheck tcfdbget");

PyDoc_STRVAR(out__doc__,
"DB.out(k) -> bool, remove k from DB. Returns True if successful.\n\nCheck tcfdbout");

PyDoc_STRVAR(put__doc__,
"DB.put(k, v) -> bool, sets k to v in DB. Returns True if successful.\n\nCheck tcfdbput");

PyDoc_STRVAR(putkeep__doc__,
"DB.putkeep(k, v) -> bool, if k not in DB sets k to v. Returns False if k in DB otherwise True.\n\nCheck tcfdbputkeep");

PyDoc_STRVAR(putcat__doc__,
"DB.putkeep(k, v) -> bool, if k not in DB sets k to v otherwise concatenate v to the existing value. Returns True if successful.\n\nCheck tcfdbputcat");

PyDoc_STRVAR(keys__doc__,
"DB.keys() -> list of DB's keys");

PyDoc_STRVAR(values__doc__,
"DB.values() -> list of DB's values");

PyDoc_STRVAR(iterkeys__doc__,
"DB.iterkeys() -> an iterator over the keys of DB");

PyDoc_STRVAR(itervalues__doc__,
"DB.itervalues() -> an iterator over the values of DB");

PyDoc_STRVAR(iteritems__doc__,
"DB.iteritems() -> an iterator over the (key, value) items of DB");


static PyMethodDef tc_fdb_methods[] = {
    {
        "__getitem__",  (PyCFunction)tc_fdb___getitem__,    METH_O | METH_COEXIST,          __getitem____doc__
    },
    {
        "__setitem__",  (PyCFunction)tc_fdb_put,            METH_VARARGS | METH_COEXIST,    __setitem____doc__
    },
    {
        "__contains__", (PyCFunction)tc_fdb___contains__,   METH_O | METH_COEXIST,          __contains____doc__
    },
    {
        "has_key",      (PyCFunction)tc_fdb___contains__,   METH_O,                         has_key__doc__
    },
    {
        "get",          (PyCFunction)tc_fdb_get,            METH_O,                         get__doc__
    },
    {
        "out",          (PyCFunction)tc_fdb_out,            METH_O,                         out__doc__
    },
    {
        "put",          (PyCFunction)tc_fdb_put,            METH_VARARGS,                   put__doc__
    },
    {
        "putkeep",      (PyCFunction)tc_fdb_putkeep,        METH_VARARGS,                   putkeep__doc__
    },
    {
        "putcat",       (PyCFunction)tc_fdb_putcat,         METH_VARARGS,                   putcat__doc__
    },
    {
        "keys",         (PyCFunction)tc_fdb_keys,           METH_NOARGS,                    keys__doc__
    },
    {
        "values",       (PyCFunction)tc_fdb_values,         METH_NOARGS,                    values__doc__
    },
    {
        "iterkeys",     (PyCFunction)tc_fdb_iter_keys,      METH_NOARGS,                    iterkeys__doc__
    },
    {
        "itervalues",   (PyCFunction)tc_fdb_iter_values,    METH_NOARGS,                    itervalues__doc__
    },
    {
        "iteritems",    (PyCFunction)tc_fdb_iter_items,     METH_NOARGS,                    iteritems__doc__
    },
    /* Sentinel */
    {
        NULL,           NULL
    }
};

static PyMappingMethods tc_fdb_as_mapping = {
    (lenfunc)tc_fdb_length,                     /* mp_length */
    (binaryfunc)tc_fdb___getitem__impl,         /* mp_subscript */
    (objobjargproc)tc_fdb_ass_subscript,        /* mp_ass_subscript */
};

/* Hack to implement "key in dict" */
static PySequenceMethods tc_fdb_as_sequence = {
    0,                                          /* sq_length */
    0,                                          /* sq_concat */
    0,                                          /* sq_repeat */
    0,                                          /* sq_item */
    0,                                          /* sq_slice */
    0,                                          /* sq_ass_item */
    0,                                          /* sq_ass_slice */
    (objobjproc)tc_fdb___contains___impl,       /* sq_contains */
    0,                                          /* sq_inplace_concat */
    0,                                          /* sq_inplace_repeat */
};

PyTypeObject PyTcFdb_Type = {
    #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    #else
    PyVarObject_HEAD_INIT(NULL, 0)
    #endif
    "tc.fdb",                                   /* tp_name */
    sizeof(TcFdbObject),                        /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)tc_fdb_dealloc,                 /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    &tc_fdb_as_sequence,                        /* tp_as_sequence */
    &tc_fdb_as_mapping,                         /* tp_as_mapping */
    tc_fdb_nohash,                              /* tp_hash  */
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
    tc_fdb_methods,                             /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)tc_fdb_init,                      /* tp_init */
    0,                                          /* tp_alloc */
    PyType_GenericNew,                          /* tp_new */
};

int tc_fdb_register(PyObject *module)
{
    if (PyType_Ready(&PyTcFdb_Type) == 0)
        return PyModule_AddObject(module, "fdb", (PyObject *)&PyTcFdb_Type);

    return -1;
}

/* Tokyo Cabinet fixed-width database iterator types */

static PyObject *
tc_fdb_iter_keys(PyObject *pyself)
{
        return tc_fdb_iter_new((TcFdbObject *)pyself, FDB_ITER_TYPE_KEY);
}

static PyObject *
tc_fdb_iter_values(PyObject *pyself)
{
        return tc_fdb_iter_new((TcFdbObject *)pyself, FDB_ITER_TYPE_VALUE);
}

static PyObject *
tc_fdb_iter_items(PyObject *pyself)
{
        return tc_fdb_iter_new((TcFdbObject *)pyself, FDB_ITER_TYPE_ITEM);
}

static PyObject *
tc_fdb_iter_new(TcFdbObject *tc_fdb, enum IterType type)
{
    TcFdbIterObject *it;
    it = PyObject_New(TcFdbIterObject, &PyTcFdbIter_Type);
    if (it == NULL)
        return NULL;
    Py_INCREF(tc_fdb);

    it->tc_fdb = tc_fdb;
    it->type = type;

    if (tcfdbiterinit(tc_fdb->db))
        return (PyObject *)it;

    // Set Exception!
    return NULL;
}

static void
tc_fdb_iter_dealloc(TcFdbIterObject *it)
{
    Py_XDECREF(it->tc_fdb);
    PyObject_Del(it);
}

static Py_ssize_t
tc_fdb_iter_len_impl(TcFdbIterObject *it)
{
    Py_ssize_t len = 0;
    if (it->tc_fdb != NULL)
        len = tc_fdb_length(it->tc_fdb);

    return len;
}

static PyObject *
tc_fdb_iter_len(TcFdbIterObject *it)
{
    return PyInt_FromSsize_t(tc_fdb_iter_len_impl(it));
}

static PyObject *
tc_fdb_iter_next(TcFdbIterObject *it)
{
    if (it->tc_fdb == NULL)
        return NULL;

    PY_LONG_LONG key;
    Py_BEGIN_ALLOW_THREADS
    key = tcfdbiternext(it->tc_fdb->db);
    Py_END_ALLOW_THREADS

    if (key != 0) {
        PyObject *pykey = NULL;
        PyObject *pyvalue = NULL;

        if (it->type == FDB_ITER_TYPE_KEY || it->type == FDB_ITER_TYPE_ITEM) {
            pykey = PyLong_FromLongLong(key);
        }
        if (it->type == FDB_ITER_TYPE_VALUE || it->type == FDB_ITER_TYPE_ITEM) {
            pyvalue = tc_fdb_get_impl(it->tc_fdb, key);
        }

        switch (it->type) {
            case FDB_ITER_TYPE_KEY:
                if (pykey != NULL)
                    return pykey;
                break;
            case FDB_ITER_TYPE_VALUE:
                if (pyvalue != NULL)
                    return pyvalue;
                break;
            case FDB_ITER_TYPE_ITEM:
                if (pykey != NULL && pyvalue != NULL) {
                    PyObject *t;
                    t = PyTuple_Pack(2, pykey, pyvalue);
                    if (t != NULL)
                        return t;
                }
                break;
        }
    }

    Py_DECREF(it->tc_fdb);
    it->tc_fdb = NULL;
    return NULL;    
}

PyDoc_STRVAR(__length_hint____doc__,
"Private method returning an estimate of len(list(it)).");

static PyMethodDef tc_fdb_iter_methods[] = {
    {
        "__length_hint__", (PyCFunction)tc_fdb_iter_len,        METH_NOARGS,                    __length_hint____doc__
    },
    /* Sentinel */
    {
        NULL,           NULL
    }
};

PyTypeObject PyTcFdbIter_Type = {
    #if (PY_VERSION_HEX < 0x03000000)
    PyObject_HEAD_INIT(&PyType_Type)
    0,                                          /* ob_size */
    #else
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    #endif
    "tc.fdb-iterator",                          /* tp_name */
    sizeof(TcFdbIterObject),                    /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)tc_fdb_iter_dealloc,            /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    0,                                          /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    PyObject_SelfIter,                          /* tp_iter */
    (iternextfunc)tc_fdb_iter_next,             /* tp_iternext */
    tc_fdb_iter_methods,                        /* tp_methods */
    0,
};
