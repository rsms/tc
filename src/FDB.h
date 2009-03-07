#ifndef PYTC_FDB_H
#define PYTC_FDB_H

#include "_base.h"
#include <tcfdb.h>

enum PutType
{
    PUT_TYPE_DEFAULT,
    PUT_TYPE_KEEP,
    PUT_TYPE_CAT
};

enum IterType
{
    FDB_ITER_TYPE_KEY,
    FDB_ITER_TYPE_VALUE,
    FDB_ITER_TYPE_ITEM
};

typedef struct {
    PyObject_HEAD
    TCFDB *db;
} TcFdbObject;

extern PyTypeObject PyTcFdb_Type;

typedef struct {
    PyObject_HEAD
    TcFdbObject *tc_fdb; /* Set to NULL when iterator is exhausted */
    enum IterType type;
} TcFdbIterObject;

extern PyTypeObject PyTcFdbIter_Type;

void tc_fdb_seterr(TcFdbObject *self);
static int tc_fdb_init(PyObject *pyself, PyObject *args, PyObject *kwds);
static void tc_fdb_dealloc(TcFdbObject *self);
static long tc_fdb_nohash(PyObject *pyself);
static int tc_fdb_out_impl(TcFdbObject* self, PyObject* pykey);
static PyObject *tc_fdb_out(PyObject* pyself, PyObject* pykey);
static PyObject *tc_fdb_get_impl(TcFdbObject* self, PY_LONG_LONG key);
static PyObject *tc_fdb_get_arg_impl(TcFdbObject* self, PyObject* pykey);
static PyObject *tc_fdb_get(PyObject* pyself, PyObject* pykey);
static PyObject *tc_fdb___getitem__impl(TcFdbObject *self, PyObject *pykey);
static PyObject *tc_fdb___getitem__(PyObject *pyself, PyObject *pykey);
static int tc_fdb_put_impl(TcFdbObject* self, PyObject* pykey, PyObject* pyvalue, enum PutType type);
static int tc_fdb_put_args_impl(TcFdbObject* self, PyObject* args, enum PutType type);
static PyObject *tc_fdb_put(PyObject* pyself, PyObject* args);
static PyObject *tc_fdb_putkeep(PyObject* pyself, PyObject* args);
static PyObject *tc_fdb_putcat(PyObject* pyself, PyObject* args);
static int tc_fdb_ass_subscript(TcFdbObject *self, PyObject *key, PyObject *value);
static Py_ssize_t tc_fdb_length(TcFdbObject *self);
static int tc_fdb___contains___impl(PyObject *pyself, PyObject *_key);
static PyObject *tc_fdb___contains__(PyObject *pyself, PyObject *key);
static PyObject *tc_fdb_list(TcFdbObject *self, enum IterType itertype);
static PyObject *tc_fdb_keys(PyObject *pyself);
static PyObject *tc_fdb_values(PyObject *pyself);
int tc_fdb_register(PyObject *module);

static PyObject *tc_fdb_iter_new(TcFdbObject *tc_fdb, enum IterType type);
static void tc_fdb_iter_dealloc(TcFdbIterObject *it);
static PyObject *tc_fdb_iter_keys(PyObject *pyself);
static PyObject *tc_fdb_iter_values(PyObject *pyself);
static PyObject *tc_fdb_iter_items(PyObject *pyself);
static Py_ssize_t tc_fdb_iter_len_impl(TcFdbIterObject *it);
static PyObject *tc_fdb_iter_len(TcFdbIterObject *it);
static PyObject *tc_fdb_iter_next(TcFdbIterObject *it);

#endif
