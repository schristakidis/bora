#include <Python.h>
#include <unistd.h>

typedef struct {
  PyObject_HEAD
  long int m;
  long int i;
} spam_MyIter;

PyObject* spam_MyIter_iter(PyObject *self)
{
  Py_INCREF(self);
  return self;
}

PyObject* spam_MyIter_iternext(PyObject *self)
{
  spam_MyIter *p = (spam_MyIter *)self;
  if (p->i < p->m) {
    PyObject *tmp = Py_BuildValue("l", p->i);
    (p->i)++;
    Py_BEGIN_ALLOW_THREADS
    sleep(1);
    Py_END_ALLOW_THREADS
    return tmp;
  } else {
    /* Raising of standard StopIteration exception with empty value. */
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
  }
}

static PyTypeObject spam_MyIterType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "spam._MyIter",            /*tp_name*/
    sizeof(spam_MyIter),       /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,
      /* tp_flags: Py_TPFLAGS_HAVE_ITER tells python to
         use tp_iter and tp_iternext fields. */
    "Internal myiter iterator object.",           /* tp_doc */
    0,  /* tp_traverse */
    0,  /* tp_clear */
    0,  /* tp_richcompare */
    0,  /* tp_weaklistoffset */
    spam_MyIter_iter,  /* tp_iter: __iter__() method */
    spam_MyIter_iternext  /* tp_iternext: next() method */
};

static PyObject *
spam_myiter(PyObject *self, PyObject *args)
{
  long int m;
  spam_MyIter *p;

  if (!PyArg_ParseTuple(args, "l", &m))  return NULL;

  /* I don't need python callable __init__() method for this iterator,
     so I'll simply allocate it as PyObject and initialize it by hand. */

  p = PyObject_New(spam_MyIter, &spam_MyIterType);
  if (!p) return NULL;

  /* I'm not sure if it's strictly necessary. */
  if (!PyObject_Init((PyObject *)p, &spam_MyIterType)) {
    Py_DECREF(p);
    return NULL;
  }

  p->m = m;
  p->i = 0;
  return (PyObject *)p;
}

int bubba = 10;


// THIS IS METHOD
static PyObject* bora(PyObject* self)
{
    int babba = bubba;
    bubba++;
    
    return Py_BuildValue("i", babba);
}


// THIS IS METHOD DOCS
static char bora_docs[] =
    "bora( ): Any message you want to put here!!\n";
    
static PyMethodDef SpamMethods[] = {
    {"myiter",  spam_myiter, METH_VARARGS, "Iterate from i=0 while i<m."},
    {"bora", (PyCFunction)bora, 
     METH_NOARGS, bora_docs},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initspam(void)
{
  PyObject* m;

  spam_MyIterType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&spam_MyIterType) < 0)  return;

  m = Py_InitModule("spam", SpamMethods);

  Py_INCREF(&spam_MyIterType);
  PyModule_AddObject(m, "_MyIter", (PyObject *)&spam_MyIterType);
}
