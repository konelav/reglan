//#include <C:\Python37\include\Python.h>
#include "Python.h"
#include <structmember.h>

#if PY_MAJOR_VERSION >= 3

#  define PyFree(v)         Py_TYPE(v)->tp_free((PyObject*)(v))
#  define ToPCSTR           PyBytes_AsString
#  define FromPCSTR         PyBytes_FromString
#  define BEGIN_TYPEDEF     PyVarObject_HEAD_INIT(NULL, 0)
#  define GetIndicesArgType PyObject

#  define MODULE_ERROR      NULL
#  define MODULE_RETURN(v)  return (v)
#  define MODULE_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
#  define MODULE_DEF(name,doc,methods) \
    static struct PyModuleDef moduledef = { \
        PyModuleDef_HEAD_INIT, (name), (doc), -1, (methods), };
#  define MODULE_CREATE(obj,name,doc,methods) \
    obj = PyModule_Create(&moduledef);

#else /* Python 2.x */

#  define PyFree(v)         (v)->ob_type->tp_free((PyObject*)(v));
#  define ToPCSTR           PyString_AsString
#  define FromPCSTR         PyString_FromString
#  define BEGIN_TYPEDEF     PyObject_HEAD_INIT(NULL) 0,
#  define GetIndicesArgType PySliceObject

#  define MODULE_ERROR
#  define MODULE_RETURN(v)
#  define MODULE_INIT(name) void init##name(void)
#  define MODULE_DEF(name,doc,methods)
#  define MODULE_CREATE(obj,name,doc,methods) \
    obj = Py_InitModule3((name), (methods), (doc));

#endif

#include "../src/reglan.h"

#include "../src/arith.c"
#include "../src/parse.c"
#include "../src/alteration.c"
#include "../src/concatenation.c"
#include "../src/print.c"


static char REGLAN_MODULE_DOC[] = "REGular LANguage generator \n\
\n\
Reglan is tiny library for generating set of words given regular \
expressions. The syntax of these expressions is pretty similar \
to that is used in `re` python module. \n\
\n\
It supports: \n\
  - all types of quantifiers (`*`, `+`, `?`, `{n}`, `{n,m}` and `{n,}`); \n\
  - character sets with ranges and negations (`[qwe]`, `[a-z]`, `[^0-9]`); \n\
  - character classes such as `.`, `\\d`, `\\S` and so on; \n\
  - escaped special chars and hex-codes such as `\\t`, `\\xDD` and so on; \n\
  - grouping with brackets and backreferences, e.g. `\\1`; \n\
  - alternatives from files. e.g. `(?F/usr/share/dict/words)` enumerates all lines from `words` file. \n\
\n\
The only thing provided by module is `reglan` generator type.";

static char REGLAN_TYPE_DOC[] = "Generator of words of regular language \n\
\n\
This generator enumerates all words that matches to given regular expression. \n\
Argument of it's constructor is as follows: \n\
    - @pattern that contains string (byte-sequence) of regular expression \n\
       and as such defines some regular language, or set of words; \n\
    - @offset that contains number of words from the beginning that must be \n\
       skipped before generating \n\
    - @count that contains number of words that might be generated; the actual \n\
       number of generated words can be less than this value, in case set of \n\
       words exhausts earlier. \n\
\n\
This generator supports `len()` call that returns number of words it \
can generate (accounting @offset and @count parameters). \n\
Also it supports subscripting with single integer `[d]` which returns d-th \
word of language, and subscripting with two integers `[start:stop]` one of \
which can abscent (`[:stop]`, `[start:]`). \n\
";

#define BUFFER_DEFAULT_SIZE     1024

typedef struct {
    PyObject_HEAD
    
    PyObject *pattern;
    
    struct SRegexpr regexp;
    struct SAlteration root;
    struct SAlteration *fast_inc;
    
    char *buffer;
    int bufsize;
    
    long long offset;
    long long count;
    long long pos;
} Reglan;

static PyTypeObject PyReglanType;

static void Reglan_dealloc(Reglan* self)
{
    Py_XDECREF(self->pattern);
    alteration_free(&self->root);
    regexp_free(&self->regexp);
    free(self->buffer);
    PyFree(self);
}

static PyObject *Reglan_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Reglan *self = NULL;
    PyObject *pattern = NULL;
    
    long long offset = 0, count = UNLIMITED;
    static char *kwlist[] = {"pattern", "offset", "count", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|SLL", kwlist, 
                                     &pattern, &offset, &count))
        return NULL; 
    
    if (offset < 0)
        offset = 0;
    if (count < 0)
        count = UNLIMITED;
    
    self = (Reglan*)type->tp_alloc(type, 0);
    if (!self)
        return NULL;
    
    Py_INCREF(pattern);

    self->pattern = pattern;
    
    self->bufsize = BUFFER_DEFAULT_SIZE;
    self->buffer = (char*)calloc(1, self->bufsize);
    
    self->offset = offset;
    self->count = count;
    self->pos = 0;
    
    parse(ToPCSTR(self->pattern), &self->regexp, &self->root);
    
    if (self->regexp.full_length != UNLIMITED && 
        self->regexp.full_length != 0) {
        self->offset %= self->regexp.full_length;
    }
    
    alteration_set_offset(&self->root, self->offset);
    
    return (PyObject*)self;
}

static PyObject *Reglan_next(Reglan* self)
{
    if (self->count != UNLIMITED && self->pos == self->count)
        return NULL;
    
    while (alteration_value(&self->root, self->buffer, self->bufsize-1) == self->bufsize-1) {
        self->bufsize += 1024;
        self->buffer = (char*)realloc(self->buffer, self->bufsize);
    }
    self->pos++;
    
    if (self->fast_inc && 
        !alteration_inc_inplace(self->fast_inc))
        self->fast_inc = NULL;
    
    if (!self->fast_inc) {
        self->fast_inc = alteration_inc(&self->root);
        if (!self->fast_inc)
            self->pos = self->count = 0;
    }
        
    return FromPCSTR(self->buffer);
}

static PyObject *Reglan_repr(Reglan* self)
{
    return PyUnicode_FromFormat("reglan(%R)", self->pattern);
}

static Py_ssize_t Reglan_length(Reglan* self)
{
    long long length = self->regexp.full_length;
    if (length != UNLIMITED)
        length -= self->offset;
    if (self->count != UNLIMITED) {
        if (length == UNLIMITED || length > self->count)
            length = self->count;
    }
    return (Py_ssize_t)length;
}

static PyObject *Reglan_subscript(Reglan *self, PyObject *key) {
    if (PyNumber_Check(key)) {
        long long index = PyLong_AsLongLong(key);
        long long offset = self->offset + self->pos;
        
        if (offset != index)
            alteration_set_offset(&self->root, index);
        
        alteration_value(&self->root, self->buffer, self->bufsize-1);
        
        if (offset != index)
            alteration_set_offset(&self->root, offset);
        
        return FromPCSTR(self->buffer);
    }
    
    if (PySlice_Check(key)) {
        Py_ssize_t length, start, stop, step, slicelength;
        
        length = Reglan_length(self);
        if (length == UNLIMITED)
            length = BIGNUM;
        
        if (PySlice_GetIndicesEx((GetIndicesArgType*)key, length, &start, &stop, &step, 
            &slicelength) != 0)
            return NULL;
        
        if (step != 1)
            return NULL;
        
        start += self->offset;
        stop += self->offset;
        
        {
            PyObject *argList = Py_BuildValue("SLLi", self->pattern, start, 
                stop - start, self->bufsize);
            PyObject *new_reglan = PyObject_CallObject((PyObject *) &PyReglanType, argList);
            Py_DECREF(argList);
            return new_reglan;
        }
    }
    return NULL;
}

static PyMappingMethods Reglan_mappingmethods = {
    (lenfunc)Reglan_length,     /* mp_length 	lenfunc 	PyObject * 	Py_ssize_t */
    (binaryfunc)Reglan_subscript, /* mp_subscript 	binaryfunc 	PyObject * PyObject * 	PyObject * */
    0,                          /* mp_ass_subscript 	objobjargproc 	PyObject * PyObject * PyObject * 	int */
};

static PyTypeObject PyReglanType = {
    BEGIN_TYPEDEF
    "reglan",                  /*tp_name*/
    sizeof(Reglan),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Reglan_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc)Reglan_repr,     /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    &Reglan_mappingmethods,    /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    (reprfunc)Reglan_repr,     /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    REGLAN_TYPE_DOC,           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    PyObject_SelfIter,         /* tp_iter */
    (iternextfunc)Reglan_next, /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    PyType_GenericAlloc,       /* tp_alloc */
    Reglan_new,                /* tp_new */
};


static PyMethodDef methods[] = {
    { NULL, NULL, 0, NULL }
};


MODULE_DEF("reglan", REGLAN_MODULE_DOC, methods);

MODULE_INIT(reglan)
{
    PyObject *m;
    
    if (PyType_Ready(&PyReglanType) < 0)
        MODULE_RETURN(NULL);
    
    MODULE_CREATE(m, "reglan", REGLAN_MODULE_DOC, methods)
    
    if (!m)
        return MODULE_ERROR;
    
    Py_INCREF(&PyReglanType);
    PyModule_AddObject(m, "reglan", (PyObject *)&PyReglanType);
    
    MODULE_RETURN(m);
}
