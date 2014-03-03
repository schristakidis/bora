#include <Python.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __WIN32__
#include <sys/stat.h>
#endif
#include <semaphore.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "biter_bridge.h"
#include "bpuller_bridge.h"
#include "blockcache.h"
#include "stats_bridge.h"
#include "packet_sender.h"
#include "packet_receiver.h"

static int sock = 0;

static int death = 0;

#ifdef __WIN32__

int init_wsa (void) {
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(nResult != NO_ERROR)
	{
		fprintf(stderr, "WSAStartup() failed.\n");
		return -1;
	}
}

#endif

typedef struct {
  PyObject_HEAD
  //
} bora_BPuller;


PyObject* bora_BPuller_iter(PyObject *self)
{
  Py_INCREF(self);
  return self;
}

PyObject* bora_BPuller_iternext(PyObject *self)
{
  //bora_BPuller *p = (bora_BPuller *)self;
  if (!death) {
    Py_BEGIN_ALLOW_THREADS
    sem_wait(&s_bpuller_full);
    sem_post(&s_bpuller_empty);
    Py_END_ALLOW_THREADS
    if (!death) {
      PyObject *tmp = Py_BuildValue("i", 1);
      return tmp;
    } else {
      PyErr_SetNone(PyExc_StopIteration);
      return NULL;
    }
  } else {
    /* Raising of standard StopIteration exception with empty value. */
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
  }
}

static PyTypeObject bora_BPullerType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "bora._BPuller",            /*tp_name*/
    sizeof(bora_BPuller),       /*tp_basicsize*/
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
    "Internal puller iterator object.",           /* tp_doc */
    0,  /* tp_traverse */
    0,  /* tp_clear */
    0,  /* tp_richcompare */
    0,  /* tp_weaklistoffset */
    bora_BPuller_iter,  /* tp_iter: __iter__() method */
    bora_BPuller_iternext  /* tp_iternext: next() method */
};

static PyObject *
bora_bpuller(PyObject *self, PyObject *args)
{
  if (!sock) {
        PyErr_SetString(PyExc_AttributeError, "Sock not open");
        return NULL;
  }

  bora_BPuller *p;

  //if (!PyArg_ParseTuple(args, "l", &m))  return NULL;

  /* I don't need python callable __init__() method for this iterator,
     so I'll simply allocate it as PyObject and initialize it by hand. */

  p = PyObject_New(bora_BPuller, &bora_BPullerType);
  if (!p) return NULL;

  /* I'm not sure if it's strictly necessary. */
  if (!PyObject_Init((PyObject *)p, &bora_BPullerType)) {
    Py_DECREF(p);
    return NULL;
  }

  //p->m = m;
  //p->i = 0;
  //p->death = 0;
  return (PyObject *)p;
}

/*
END OF BLOCK REQUESTER
*/

typedef struct {
  PyObject_HEAD
  //
} bora_BIter;


PyObject* bora_BIter_iter(PyObject *self)
{
  Py_INCREF(self);
  return self;
}

PyObject* bora_BIter_iternext(PyObject *self)
{
  bora_BIter *p = (bora_BIter *)self;
  if (!death) {
    int s, b;
    int c = 0;

    //(p->i)++;
    Py_BEGIN_ALLOW_THREADS
    sem_wait(&s_biter_full);
    s = b_biter_s[c];
    b = b_biter_b[c];
    c = (c+1)%N_BITER;
    sem_post(&s_biter_empty);
    Py_END_ALLOW_THREADS
    if (!death) {
      PyObject *tmp = Py_BuildValue("ii", s, b);
      return tmp;
    } else {
      /* Raising of standard StopIteration exception with empty value. */
      PyErr_SetNone(PyExc_StopIteration);
      return NULL;
    }
  } else {
    /* Raising of standard StopIteration exception with empty value. */
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
  }
}

static PyTypeObject bora_BIterType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "bora._BIter",            /*tp_name*/
    sizeof(bora_BIter),       /*tp_basicsize*/
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
    "Internal biter iterator object.",           /* tp_doc */
    0,  /* tp_traverse */
    0,  /* tp_clear */
    0,  /* tp_richcompare */
    0,  /* tp_weaklistoffset */
    bora_BIter_iter,  /* tp_iter: __iter__() method */
    bora_BIter_iternext  /* tp_iternext: next() method */
};

static PyObject *
bora_biter(PyObject *self, PyObject *args)
{
  if (!sock) {
        PyErr_SetString(PyExc_AttributeError, "Sock not open");
        return NULL;
  }

  bora_BIter *p;

  //if (!PyArg_ParseTuple(args, "l", &m))  return NULL;

  /* I don't need python callable __init__() method for this iterator,
     so I'll simply allocate it as PyObject and initialize it by hand. */

  p = PyObject_New(bora_BIter, &bora_BIterType);
  if (!p) return NULL;

  /* I'm not sure if it's strictly necessary. */
  if (!PyObject_Init((PyObject *)p, &bora_BIterType)) {
    Py_DECREF(p);
    return NULL;
  }

  //p->m = m;
  //p->i = 0;
  //p->death = 0;
  return (PyObject *)p;
}

static PyObject* die(PyObject* self, PyObject * value )
{
    if (!sock) {
      PyErr_SetString(PyExc_AttributeError, "Sock is not open. Generators not running?");
      return NULL;
    }
    death = 1;
    sem_post(&s_bpuller_full);
    sem_post(&s_biter_full);
    if (close(sock)==-1) {
      perror("Could not close socket");
      Py_RETURN_NONE;
    }
    sock = 0;
    Py_RETURN_TRUE;
}

static PyObject *listen_on( PyObject * self, PyObject * value )
{
        if (sock) {
                PyErr_SetString(PyExc_AttributeError, "Sock already open");
                return NULL;
        }

        int port = (int)PyInt_AsLong(value);

        if( port==-1 && PyErr_Occurred() ) {
                PyErr_SetString(PyExc_AttributeError, "not an int");
                return NULL;
        } else if (port>65535 || port<1) {
                PyErr_SetString(PyExc_AttributeError, "port has to be bigger than 0 and smaller than 65536");
                return NULL;
        }

        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (!sock || sock==-1) {
                PyErr_SetString(PyExc_AttributeError, "Can't create socket");
                return NULL;
        }

        struct sockaddr_in servaddr = {0};
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
        servaddr.sin_port=htons(port);
        if (bind(sock, (struct sockaddr*)&servaddr, sizeof(servaddr))!=0) {
          sock = 0;
          perror("CANT BIND:");
          PyErr_SetString(PyExc_AttributeError, "Can't bind");
          return NULL;
        }

        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //TODO CHECK WIN32

        init_sender(sock);
        init_receiver(sock);
        init_biter();
        init_bpuller();


        Py_RETURN_NONE;
}

static PyObject *send_block( PyObject * self, PyObject * args )
{
    if (!sock) {
            PyErr_SetString(PyExc_AttributeError, "Sock not open");
            return NULL;
    }

    int s_id;
    int b_id;
    unsigned char * dest;
    int dest_len;
    int port_num;

    int s;

    struct sockaddr_in destaddr = {0};

    if (!PyArg_ParseTuple(args, "iis#i", &s_id, &b_id, &dest, &dest_len, &port_num)) {
                PyErr_SetString(PyExc_AttributeError, "Wrong arguments");
                return NULL;
    }

    destaddr.sin_family = AF_INET;

    #ifdef __WIN32__
    destaddr.sin_addr.s_addr = s = inet_addr((const char*)dest);
    #else
    s = inet_pton(AF_INET, (const char*)dest, &(destaddr.sin_addr));
    #endif

    if (s <= 0) {
               if (s == 0) {
                  PyErr_SetString(PyExc_AttributeError, "DEST IP: Not in presentation format");
                  return NULL;
               } else {
                  PyErr_SetString(PyExc_AttributeError, "DEST IP: Not in presentation format");
                  perror("inet_pton");
                  return NULL;
               }
    }

    if (port_num>65535 || port_num<1) {
                  PyErr_SetString(PyExc_AttributeError, "Port number not allowed");
                  return NULL;
    }
    destaddr.sin_port=htons(port_num);

    if (sendblock(s_id, b_id, destaddr)) {
      Py_RETURN_TRUE;
    }

    Py_RETURN_NONE;
}

static PyObject *add_block( PyObject * self, PyObject * args )
{
    int s_id;
    int b_id;
    BlockData b_data;

    if (!PyArg_ParseTuple(args, "iis#", &s_id, &b_id, &b_data.data, &b_data.length)) {
                PyErr_SetString(PyExc_AttributeError, "Wrong arguments");
                return NULL;
    }

    if (addblock((uint16_t)s_id, (uint32_t)b_id, &b_data)) {
      Py_RETURN_TRUE;
    }

    Py_RETURN_NONE;
}

static PyObject *del_block( PyObject * self, PyObject * args )
{
    int s_id;
    int b_id;
    int retval;

    if (!PyArg_ParseTuple(args, "ii", &s_id, &b_id)) {
                PyErr_SetString(PyExc_AttributeError, "Wrong arguments");
                return NULL;
    }

    retval = deleteblock((uint16_t)s_id, (uint32_t)b_id);

    if (retval) {
      Py_RETURN_TRUE;
    }

    Py_RETURN_NONE;
}

static PyObject *incomplete_block_list( PyObject * self, PyObject * args )
{
    int i;
    PyObject* ret;
    BlockIDList blist = get_incomplete_block_list();

    ret = PyList_New((Py_ssize_t)blist.length);
    for (i=0; i<blist.length; i++) {
      PyList_SET_ITEM(ret, i, Py_BuildValue("{sisi}", "sid", (uint16_t)blist.blist[i].streamid, "bid", (uint32_t)blist.blist[i].blockid));
    }

    if (blist.blist!=NULL) {
      free(blist.blist);
    }

    return ret;
}

static PyObject *complete_block_list( PyObject * self, PyObject * args )
{
    int i;
    PyObject* ret;
    BlockIDList blist = get_complete_block_list();

    ret = PyList_New((Py_ssize_t)blist.length);
    for (i=0; i<blist.length; i++) {
      PyList_SET_ITEM(ret, i, Py_BuildValue("{sisi}", "sid", blist.blist[i].streamid, "bid", blist.blist[i].blockid));
    }

    if (blist.blist!=NULL) {
      free(blist.blist);
    }

    return ret;
}

static PyObject *get_in_stats( PyObject * self, PyObject * value )
{

    int rst = (int)PyInt_AsLong(value);
    PyObject* ret;

    ret = PyDict_New();

    pthread_mutex_lock(&stat_lock_r);
    PyDict_SetItemString(ret, "I_PKG_COUNTER", Py_BuildValue("i", stats_r[I_PKG_COUNTER]));
    PyDict_SetItemString(ret, "I_DATA_COUNTER", Py_BuildValue("i", stats_r[I_DATA_COUNTER]));
    PyDict_SetItemString(ret, "I_ACK_DATA_COUNTER", Py_BuildValue("i", stats_r[I_ACK_DATA_COUNTER]));
    PyDict_SetItemString(ret, "I_ACK_COUNTER", Py_BuildValue("i", stats_r[I_ACK_COUNTER]));
    PyDict_SetItemString(ret, "I_GARBAGE", Py_BuildValue("i", stats_r[I_GARBAGE]));
    PyDict_SetItemString(ret, "I_DUPE_COUNTER", Py_BuildValue("i", stats_r[I_DUPE_COUNTER]));
    PyDict_SetItemString(ret, "I_DUPE_DATA_COUNTER", Py_BuildValue("i", stats_r[I_DUPE_DATA_COUNTER]));
    if (rst) {
      reset_in_counters();
    }
    pthread_mutex_unlock(&stat_lock_r);


    return ret;
}

static PyObject *get_out_stats( PyObject * self, PyObject * value )
{

    int rst = (int)PyInt_AsLong(value);
    PyObject* ret;

    ret = PyDict_New();

    pthread_mutex_lock(&stat_lock_s);
    PyDict_SetItemString(ret, "O_DATA_COUNTER", Py_BuildValue("i", stats_s[O_DATA_COUNTER]));
    PyDict_SetItemString(ret, "O_PKG_COUNTER", Py_BuildValue("i", stats_s[O_PKG_COUNTER]));
    PyDict_SetItemString(ret, "O_ACK_COUNTER", Py_BuildValue("i", stats_s[O_ACK_COUNTER]));
    PyDict_SetItemString(ret, "O_ACK_DATA_COUNTER", Py_BuildValue("i", stats_s[O_ACK_DATA_COUNTER]));
    if (rst) {
      reset_in_counters();
    }
    pthread_mutex_unlock(&stat_lock_s);


    return ret;
}

static PyObject *get_block_content( PyObject * self, PyObject * args )
{
    int s_id;
    int b_id;
    BlockData * bdata;
    PyObject* r;


    if (!PyArg_ParseTuple(args, "ii", &s_id, &b_id)) {
                PyErr_SetString(PyExc_AttributeError, "Wrong arguments");
                return NULL;
    }

    bdata = get_block_data(s_id, b_id);
    if (!bdata) {
      Py_RETURN_NONE;
    }

    r = (PyObject* )Py_BuildValue("s#", bdata->data, bdata->length);
    free(bdata->data);
    free(bdata);


    return r;
}

// THIS IS METHOD DOCS
static char listen_docs[] =
    "listen_on( port ): Setup socket and listen on port\n";
static char in_stats_docs[] =
    "get_in_stats( reset ): Get incoming stats, if reset is not 0 counters get reset\n";
static char out_stats_docs[] =
    "get_out_stats( reset ): Get outgoing stats, if reset is not 0 counters get reset\n";
static char die_docs[] =
    "die(  ): Kill generators\n";
static char biter_docs[] =
    "biter( ): Create block recv iterator\n";
static char bpuller_docs[] =
    "bpuller( ): Create block pull iterator\n";
static char send_block_docs[] =
    "send_block( streamid, blockid, ip, port ): Send a block to the given address\n";
static char add_block_docs[] =
    "add_block( streamid, blockid, content ): Put an entire block in block cache\n";
static char del_block_docs[] =
    "del_block( streamid, blockid ): Delete a block from the block cache\n";
static char incomplete_block_list_docs[] =
    "incomplete_block_list( ): Retrieve list of incomplete blocks\n";
static char complete_block_list_docs[] =
    "complete_block_list( ): Retrieve list of complete blocks\n";
static char get_block_content_docs[] =
    "get_block_content( streamid, blockid ): Retrieve content of the given block or None\n";



static PyMethodDef BoraMethods[] = {
    {"biter", bora_biter, METH_NOARGS, biter_docs},
    {"bpuller", bora_bpuller, METH_NOARGS, bpuller_docs},
    {"listen_on", listen_on, METH_O, listen_docs},
    {"get_in_stats", get_in_stats, METH_O, in_stats_docs},
    {"get_out_stats", get_out_stats, METH_O, out_stats_docs},
    {"die", die, METH_NOARGS, die_docs},
    {"send_block", send_block, METH_VARARGS, send_block_docs},
    {"add_block", add_block, METH_VARARGS, add_block_docs},
    {"del_block", del_block, METH_VARARGS, del_block_docs},
    {"incomplete_block_list", incomplete_block_list, METH_NOARGS, incomplete_block_list_docs},
    {"complete_block_list", complete_block_list, METH_NOARGS, complete_block_list_docs},
    {"get_block_content", get_block_content, METH_VARARGS, get_block_content_docs},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initbora(void)
{
  PyObject* m;

  bora_BIterType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&bora_BIterType) < 0)  return;

  bora_BPullerType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&bora_BPullerType) < 0)  return;

  #ifdef __WIN32__
  init_wsa();
  #endif

  m = Py_InitModule("bora", BoraMethods);

  Py_INCREF(&bora_BIterType);
  PyModule_AddObject(m, "_BIter", (PyObject *)&bora_BIterType);

  Py_INCREF(&bora_BPullerType);
  PyModule_AddObject(m, "_BPuller", (PyObject *)&bora_BPullerType);
}
