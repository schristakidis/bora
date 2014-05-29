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
#include "recv_stats.h"
#include "queue.h"
#include "ack_received.h"
#include "bw_stats.h"
#include "bw_msgs.h"
#include "cookie_sender.h"

static int sock = 0;

static int death = 0;

static int c = 0;

static int bws_running = 0;

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
    //puts("BPULLER UNLOCKING\n");
    Py_BEGIN_ALLOW_THREADS
    sem_wait(&s_bpuller_full);
    sem_post(&s_bpuller_empty);
    Py_END_ALLOW_THREADS
    //puts("BPULLER LOCKING\n");
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
  //bora_BIter *p = (bora_BIter *)self;
  if (!death) {
    int s, b;

    //(p->i)++;
    //puts("BITER LOCKING\n");
    Py_BEGIN_ALLOW_THREADS
    sem_wait(&s_biter_full);
    s = b_biter_s[c];
    b = b_biter_b[c];
    c = (c+1)%N_BITER;
    sem_post(&s_biter_empty);
    Py_END_ALLOW_THREADS
    //puts("BITER UNLOCKING\n");
    //puts("GOTTABLOCK");
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

typedef struct {
  PyObject_HEAD

} bora_BWIter;


PyObject* bora_BWIter_iter(PyObject *self)
{
  Py_INCREF(self);
  return self;
}

PyObject* bora_BWIter_iternext(PyObject *self)
{

  //bora_BWIter *p = (bora_BWIter *)self;
  if (!death) {
    //puts("BWS_UNLOCKING\n");
    Py_BEGIN_ALLOW_THREADS
    sem_wait(&s_bws_hasdata);
      //puts("BORA HASDATA\n");
    Py_END_ALLOW_THREADS
    //puts("BWS_LOCKING\n");
      //puts("BORA END_THREADS\n");
    if (!death) {
      //puts("BORA PUPPA\n");
      PyObject * ret;
      PyObject * sent_data;
      PyObject * peer_stats;
      PyObject * peer_values;
      PyObject * peer_dict;
      PyObject * peer_stats_dict;
      PyObject * last_seq_dict;
      PyObject * current_seq;
      struct PeerAckStats ackstats;
      PeerAckStore * peercur;
      AckStore * peerscur;
      AckStore * tmppeerscur;
      char ip_addr[INET_ADDRSTRLEN];

      peer_stats = PyList_New(0);
      ackstats = get_ack_store();
      SLIST_FOREACH(peercur, ackstats.peerstats, entries) {
        peer_dict = PyDict_New();
        inet_ntop(AF_INET, &(peercur->addr.sin_addr), ip_addr, INET_ADDRSTRLEN);
        PyDict_SetItemString(peer_dict, "host", Py_BuildValue("s", ip_addr));
        PyDict_SetItemString(peer_dict, "port", Py_BuildValue("i", ntohs((peercur->addr).sin_port)));
        PyDict_SetItemString(peer_dict, "minSTT", Py_BuildValue("l", peercur->minSTT.tv_sec * 1000000L + peercur->minSTT.tv_usec));
        PyDict_SetItemString(peer_dict, "minRTT", Py_BuildValue("l", peercur->minRTT.tv_sec * 1000000L + peercur->minRTT.tv_usec));
        PyDict_SetItemString(peer_dict, "avgRTT", Py_BuildValue("l", peercur->avgRTT.tv_sec * 1000000L + peercur->avgRTT.tv_usec));
        PyDict_SetItemString(peer_dict, "avgSTT", Py_BuildValue("l", peercur->avgSTT.tv_sec * 1000000L + peercur->avgSTT.tv_usec));
        PyDict_SetItemString(peer_dict, "errRTT", Py_BuildValue("l", peercur->errRTT.tv_sec * 1000000L + peercur->errRTT.tv_usec));
        PyDict_SetItemString(peer_dict, "errSTT", Py_BuildValue("l", peercur->errSTT.tv_sec * 1000000L + peercur->errSTT.tv_usec));
        PyDict_SetItemString(peer_dict, "acked_total", Py_BuildValue("l", peercur->total_acked));
        PyDict_SetItemString(peer_dict, "error_total", Py_BuildValue("l", peercur->total_errors));
        PyDict_SetItemString(peer_dict, "acked_last", Py_BuildValue("l", peercur->last_acked));
        PyDict_SetItemString(peer_dict, "error_last", Py_BuildValue("l", peercur->last_error));
        peer_values = PyList_New(0);
        SLIST_FOREACH_SAFE(peerscur, &peercur->ack_store, entries, tmppeerscur) {
          peer_stats_dict = PyDict_New();
          PyDict_SetItemString(peer_stats_dict, "sent", Py_BuildValue("d", (double)peerscur->sent.tv_sec + (double)peerscur->sent.tv_usec/1000000.0));
          PyDict_SetItemString(peer_stats_dict, "RTT", Py_BuildValue("l", peerscur->RTT.tv_sec * 1000000L + peerscur->RTT.tv_usec));
          PyDict_SetItemString(peer_stats_dict, "STT", Py_BuildValue("l", peerscur->STT.tv_sec * 1000000L + peerscur->STT.tv_usec));
          PyDict_SetItemString(peer_stats_dict, "seq", Py_BuildValue("i", peerscur->seq));
          PyDict_SetItemString(peer_stats_dict, "sleep", Py_BuildValue("i", peerscur->sleeptime));
          PyList_Append(peer_values, peer_stats_dict);

          SLIST_REMOVE(&peercur->ack_store, peerscur, AckStore, entries);
          if (peerscur != ackstats.last_seq) {
            free(peerscur);
          }
        }
        PyDict_SetItemString(peer_dict, "values", Py_BuildValue("O", peer_values));
        PyList_Append(peer_stats, peer_dict);
      }

      if (PyList_Size(peer_stats)>0 && ackstats.last_seq!=NULL) {
        AckStore * ls = ackstats.last_seq;
        last_seq_dict = PyDict_New();
        inet_ntop(AF_INET, &(ls->addr->sin_addr), ip_addr, INET_ADDRSTRLEN);
        PyDict_SetItemString(last_seq_dict, "host", Py_BuildValue("s", ip_addr));
        PyDict_SetItemString(last_seq_dict, "port", Py_BuildValue("i", ntohs(ls->addr->sin_port)));
        PyDict_SetItemString(last_seq_dict, "sent", Py_BuildValue("d", (double)ls->sent.tv_sec + (double)ls->sent.tv_usec/1000000.0));
        PyDict_SetItemString(last_seq_dict, "RTT", Py_BuildValue("l", ackstats.last_seq->RTT.tv_sec * 1000000L + ackstats.last_seq->RTT.tv_usec));
        PyDict_SetItemString(last_seq_dict, "STT", Py_BuildValue("l", ackstats.last_seq->STT.tv_sec * 1000000L + ackstats.last_seq->STT.tv_usec));
        PyDict_SetItemString(last_seq_dict, "seq", Py_BuildValue("i", ackstats.last_seq->seq));
        PyDict_SetItemString(last_seq_dict, "sleep", Py_BuildValue("i", ackstats.last_seq->sleeptime));
        free(ls);
      } else {
        last_seq_dict = Py_BuildValue("");
      }

      current_seq = Py_BuildValue("i", get_seq());

      sent_data = PyDict_New();

      PyDict_SetItemString(sent_data, "O_DATA_COUNTER", Py_BuildValue("i", stats_s[O_DATA_COUNTER]));
      PyDict_SetItemString(sent_data, "O_PKG_COUNTER", Py_BuildValue("i", stats_s[O_PKG_COUNTER]));
      PyDict_SetItemString(sent_data, "O_ACK_COUNTER", Py_BuildValue("i", stats_s[O_ACK_COUNTER]));
      PyDict_SetItemString(sent_data, "O_ACK_DATA_COUNTER", Py_BuildValue("i", stats_s[O_ACK_DATA_COUNTER]));


      ret = PyDict_New();
      PyDict_SetItemString(ret, "sent_data", sent_data);
      PyDict_SetItemString(ret, "peer_stats", peer_stats);
      PyDict_SetItemString(ret, "last_ack", last_seq_dict);
      PyDict_SetItemString(ret, "last_nack", current_seq);
      PyDict_SetItemString(ret, "idle", Py_BuildValue("l", get_idle()));


      //puts("REEET");
      return ret;
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


static PyTypeObject bora_BWIterType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "bora._BWIter",            /*tp_name*/
    sizeof(bora_BWIter),       /*tp_basicsize*/
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
    "Internal bw iterator object.",           /* tp_doc */
    0,  /* tp_traverse */
    0,  /* tp_clear */
    0,  /* tp_richcompare */
    0,  /* tp_weaklistoffset */
    bora_BWIter_iter,  /* tp_iter: __iter__() method */
    bora_BWIter_iternext  /* tp_iternext: next() method */
};

static PyObject *
bora_bwiter(PyObject *self, PyObject *value)
{
  if (!sock) {
        PyErr_SetString(PyExc_AttributeError, "Sock not open");
        return NULL;
  }

  int interval = (int)PyInt_AsLong(value);

  if( interval==-1 && PyErr_Occurred() ) {
     PyErr_SetString(PyExc_AttributeError, "not an int");
     return NULL;
  }

  init_bws(interval);
  bws_running = 1;

  bora_BWIter *p;

  p = PyObject_New(bora_BWIter, &bora_BWIterType);
  if (!p) return NULL;

  /* I'm not sure if it's strictly necessary. */
  if (!PyObject_Init((PyObject *)p, &bora_BWIterType)) {
    Py_DECREF(p);
    return NULL;
  }

  return (PyObject *)p;
}




static PyObject* die(PyObject* self, PyObject * value )
{
    if (!sock) {
      PyErr_SetString(PyExc_AttributeError, "Sock is not open. Generators not running?");
      return NULL;
    }
    death = 1;
    if (bws_running) { sem_post(&s_bws_hasdata); bws_running = 0;}
    sem_post(&s_bpuller_full);
    sem_post(&s_biter_full);
    if (close(sock)==-1) {
      perror("Could not close socket");
      Py_RETURN_NONE;
    }
    sock = 0;
    Py_RETURN_TRUE;
}

static PyObject *listen_on( PyObject * self, PyObject * value)
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
        init_recv_stats();


        Py_RETURN_NONE;
}

static PyObject *send_block( PyObject * self, PyObject * args )
{
    if (death) {
        Py_RETURN_NONE;
    }
    //puts("SENDBLOCK_LOCKING\n");
    if (!sock) {
            PyErr_SetString(PyExc_AttributeError, "Sock not open");
            return NULL;
    }

    int s_id;
    int b_id;
    unsigned char * dest;
    int dest_len;
    int port_num;
    int retval;

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

    retval = sendblock(s_id, b_id, destaddr);

    if (retval) {
      Py_RETURN_TRUE;
    }

    Py_RETURN_NONE;
}

static PyObject *add_block( PyObject * self, PyObject * args )
{
    if (death) {
        Py_RETURN_NONE;
    }
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
    if (death) {
        Py_RETURN_NONE;
    }
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
    if (death) {
        Py_RETURN_NONE;
    }
    //puts("INCOMPLETE_BL_LOCKING\n");
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
    if (death) {
        Py_RETURN_NONE;
    }
    //puts("COMPLETE_BL_LOCKING\n");
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

    if (death) {
        Py_RETURN_NONE;
    }
    //puts("GET IN STATS_LOCKING\n");
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

    if (death) {
        Py_RETURN_NONE;
    }
    //puts("GET OUT STATS_LOCKING\n");
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

static PyObject *get_bw_stats( PyObject * self, PyObject * args )
{
    if (death) {
        Py_RETURN_NONE;
    }
    //puts("GET BW STATS_LOCKING\n");
    if (!sock) {
            PyErr_SetString(PyExc_AttributeError, "Sock not open");
            return NULL;
    }
    int i, j;
    PyObject* ret;
    BWEstimation * band;
    BWEstimation * band_temp;
    BW * b;
    BW * b_temp;
    struct bwstruct bwlist = fetch_bw_estimations();

    i = 0;
    SLIST_FOREACH(band, &bwlist, entries)
      i++;

    ret = PyList_New((Py_ssize_t)i);

    i = 0;
    SLIST_FOREACH_SAFE(band, &bwlist, entries, band_temp) {



        j = 0;
        SLIST_FOREACH(b, &band->bandwidth, entries)
            j++;

        PyObject* l = PyList_New((Py_ssize_t)j);

        j = 0;
        SLIST_FOREACH_SAFE(b, &band->bandwidth, entries, b_temp) {
            PyList_SET_ITEM(l, j, Py_BuildValue("{sksd}", "bw", (unsigned long int)b->bw, "tv", (double)b->tv.tv_sec + (double)b->tv.tv_usec/1000000.0));
            SLIST_REMOVE(&band->bandwidth, b, BW, entries);
            free(b);
            j++;
        }


        char ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(band->from.sin_addr), ip_addr, INET_ADDRSTRLEN);

        PyList_SET_ITEM(ret, i, Py_BuildValue("{sssHsO}", "ip", ip_addr, "port", ntohs(band->from.sin_port), "list", l));
        SLIST_REMOVE(&bwlist, band, BWEstimation, entries);
        free(band);

        i++;

    }


    return ret;
}

static PyObject *bora_get_bw_msg( PyObject * self, PyObject * args )
{
    if (death) {
        Py_RETURN_NONE;
    }
    //puts("GET BW MSG_LOCKING\n");
    if (!sock) {
            PyErr_SetString(PyExc_AttributeError, "Sock not open");
            return NULL;
    }
    PyObject* ret;
    BWMsg * b;
    BWMsg * b_temp;
    struct BandwidthMessages bwlist = get_bwmsg_list();

    ret = PyList_New(0);
    SLIST_FOREACH_SAFE(b, &bwlist, entries, b_temp) {
        char ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(b->addr.sin_addr), ip_addr, INET_ADDRSTRLEN);
        PyList_Append(ret, Py_BuildValue("{sssHsksd}", "ip", ip_addr, "port", ntohs(b->addr.sin_port), "bw", (unsigned long int)b->bw, "tv", (double)b->recv_time.tv_sec + (double)b->recv_time.tv_usec/1000000.0));

        SLIST_REMOVE(&bwlist, b, BWMsg, entries);
        free(b);
    }

    return ret;
}
static PyObject *bora_send_bw_msg( PyObject * self, PyObject * args )
{
    if (death) {
        Py_RETURN_NONE;
    }
    //puts("SEND BW MSG_LOCKING\n");
    if (!sock) {
            PyErr_SetString(PyExc_AttributeError, "Sock not open");
            return NULL;
    }

    PyObject * pylist;
    PyObject * dict_cur;
    Py_ssize_t list_len;
    Py_ssize_t i;

    struct sockaddr_in addr;
    int s;


    if (!PyArg_ParseTuple(args, "O", &pylist)) {
                PyErr_SetString(PyExc_AttributeError, "Wrong arguments");
                return NULL;
    }

    if (!PyList_Check(pylist)) {
                PyErr_SetString(PyExc_AttributeError, "First argument is not a list");
                return NULL;

    }

    list_len = PyList_Size(pylist);
    for (i=0;i<list_len; i++) {
        dict_cur = PyList_GetItem(pylist, i);
        if (!PyDict_Check(dict_cur)) {
                PyErr_SetString(PyExc_AttributeError, "List ha no-dict objects");
                return NULL;

        }
        if ( (!PyDict_GetItemString(dict_cur, "ip")) || (!PyDict_GetItemString(dict_cur, "port")) || (!PyDict_GetItemString(dict_cur, "bw"))) {
                PyErr_SetString(PyExc_AttributeError, "bad dict entries");
                return NULL;

        }

        addr.sin_family = AF_INET;

    #ifdef __WIN32__
        addr.sin_addr.s_addr = s = inet_addr((const char*)PyString_AsString(PyDict_GetItemString("ip"));
    #else

        s = inet_pton(AF_INET, (const char*)PyString_AsString(PyDict_GetItemString(dict_cur, "ip")), &(addr.sin_addr));
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

        if (!PyInt_Check(PyDict_GetItemString(dict_cur, "port"))) {
                PyErr_SetString(PyExc_AttributeError, "PORT is not INT");
                return NULL;
        }

        addr.sin_port = htons((uint16_t)PyInt_AsLong(PyDict_GetItemString(dict_cur, "port")));

        if (!PyInt_Check(PyDict_GetItemString(dict_cur, "bw"))) {
                PyErr_SetString(PyExc_AttributeError, "BW is not INT");
                return NULL;
        }

        send_bwmsg(addr, (uint32_t)PyInt_AsLong(PyDict_GetItemString(dict_cur, "bw")));

        Py_RETURN_TRUE;

    }



      Py_RETURN_NONE;
}

static PyObject *get_block_content( PyObject * self, PyObject * args )
{
    if (death) {
        Py_RETURN_NONE;
    }
    //puts("BLOCKCONTENT_LOCKING\n");
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

static PyObject *bora_bws_set( PyObject * self, PyObject * args )
{
    if (death) {
        Py_RETURN_NONE;
    }
    //puts("BW SET_LOCKING\n");
    int rst;
    int bws_time;

    if (!PyArg_ParseTuple(args, "ii", &rst, &bws_time)) {
                PyErr_SetString(PyExc_AttributeError, "Wrong arguments");
                return NULL;
    }

    set_bws_interval(bws_time);
    bws_return_value(rst);
    //puts("BWS SETTTTT\n");
    Py_RETURN_NONE;

}


static PyObject *bora_send_cookie( PyObject * self, PyObject * args )
{
    PyObject * res;
    PyObject * dict_ck;


    unsigned char * dest1;
    int dest1_len;
    int port1_num;
    unsigned char * dest2;
    int dest2_len;
    int port2_num;
    struct timespec ckTimeout = {.tv_sec = 1, .tv_nsec = 0};

    int sem_res;
    int i;

    int s;
    int t;


    struct sockaddr_in destaddr1 = {0};
    struct sockaddr_in destaddr2 = {0};

    if (!PyArg_ParseTuple(args, "s#is#i", &dest1, &dest1_len, &port1_num, &dest2, &dest2_len, &port2_num)) {
                PyErr_SetString(PyExc_AttributeError, "Wrong arguments");
                return NULL;
    }
    destaddr1.sin_family = AF_INET;
    destaddr2.sin_family = AF_INET;

    #ifdef __WIN32__
    destaddr1.sin_addr.s_addr = s = inet_addr((const char*)dest1);
    destaddr2.sin_addr.s_addr = s = inet_addr((const char*)dest2);
    #else
    s = inet_pton(AF_INET, (const char*)dest1, &(destaddr1.sin_addr));
    t = inet_pton(AF_INET, (const char*)dest2, &(destaddr2.sin_addr));
    #endif

    if (s <= 0 || t <= 0) {
               if (s == 0 || t == 0) {
                  PyErr_SetString(PyExc_AttributeError, "DEST IP: Not in presentation format");
                  return NULL;
               } else {
                  PyErr_SetString(PyExc_AttributeError, "DEST IP: Not in presentation format");
                  perror("inet_pton");
                  return NULL;
               }
    }

    if (port1_num>65535 || port1_num<1 || port2_num>65535 || port2_num<1) {
                  PyErr_SetString(PyExc_AttributeError, "Port number not allowed");
                  return NULL;
    }
    destaddr1.sin_port=htons(port1_num);
    destaddr2.sin_port=htons(port2_num);

    res = PyList_New(0);

    send_cookie(&destaddr1, &destaddr2);

    //puts("SEND COOKIE UNLOCKING\n");
    Py_BEGIN_ALLOW_THREADS
    sem_res = sem_timedwait(&ckEmpty, &ckTimeout);
    if (sem_res == 0) {
        for (i=0; i<2; i++) {
            dict_ck = PyDict_New();
            if (i==1) {
            PyDict_SetItemString(dict_ck, "host", Py_BuildValue("s#", dest1, dest1_len));
            PyDict_SetItemString(dict_ck, "port", Py_BuildValue("i", port1_num));
            } else {
            PyDict_SetItemString(dict_ck, "host", Py_BuildValue("s#", dest2, dest2_len));
            PyDict_SetItemString(dict_ck, "port", Py_BuildValue("i", port2_num));
            }
            PyDict_SetItemString(dict_ck, "sent", Py_BuildValue("d", (double)ckResult[i].sent.tv_sec + (double)ckResult[i].sent.tv_usec/1000000.0));
            PyDict_SetItemString(dict_ck, "RTT", Py_BuildValue("l", ckResult[i].RTT.tv_sec * 1000000L + ckResult[i].RTT.tv_usec));
            PyDict_SetItemString(dict_ck, "STT", Py_BuildValue("l", ckResult[i].STT.tv_sec * 1000000L + ckResult[i].STT.tv_usec));
            PyDict_SetItemString(dict_ck, "seq", Py_BuildValue("i", ckResult[i].seq));
            PyDict_SetItemString(dict_ck, "sleep", Py_BuildValue("i", ckResult[i].sleeptime));
            PyList_Append(res, dict_ck);
        }
        sem_post(&ckEmpty);
    } else {
        cookie_cleanup();
    }

    Py_END_ALLOW_THREADS
    //puts("SEND COOKIE LOCKING\n");


    return res;

}


// THIS IS METHOD DOCS
static char listen_docs[] =
    "listen_on( port ): Setup socket and listen on port\n";
static char in_stats_docs[] =
    "get_in_stats( reset ): Get incoming stats, if reset is not 0 counters get reset\n";
static char out_stats_docs[] =
    "get_out_stats( reset ): Get outgoing stats, if reset is not 0 counters get reset\n";
static char bw_stats_docs[] =
    "get_bw_stats( ): Get bandwidth stats, stats are reset each time function is called\n";
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
static char bwiter_docs[] =
    "bwsiter( interval ): Create BWS iterator returning BWS data each \"interval\" usecs\n";
static char bws_set_docs[] =
    "bws_set( bandwidth , interval): Set bandwidth in Bytes/sec, interval in usecs and restore the sending thread\n";
static char send_bw_msg_docs[] =
    "send_bw_msg( [list of {ip, port, bw}] ): Send bandwidth estimation messages to the peers\n";
static char get_bw_msg_docs[] =
    "get_bw_msg( ): Get bandwidth estimation messages received from the peers\n";
static char send_cookie_docs[] =
    "send_cookie( address1, port1, address2, port2 ): Send cookie packets to two peers\n";



static PyMethodDef BoraMethods[] = {
    {"biter", bora_biter, METH_NOARGS, biter_docs},
    {"bpuller", bora_bpuller, METH_NOARGS, bpuller_docs},
    {"listen_on", listen_on, METH_O, listen_docs},
    {"get_in_stats", get_in_stats, METH_O, in_stats_docs},
    {"get_out_stats", get_out_stats, METH_O, out_stats_docs},
    {"get_bw_stats", get_bw_stats, METH_NOARGS, bw_stats_docs},
    {"die", die, METH_NOARGS, die_docs},
    {"send_block", send_block, METH_VARARGS, send_block_docs},
    {"add_block", add_block, METH_VARARGS, add_block_docs},
    {"del_block", del_block, METH_VARARGS, del_block_docs},
    {"incomplete_block_list", incomplete_block_list, METH_NOARGS, incomplete_block_list_docs},
    {"complete_block_list", complete_block_list, METH_NOARGS, complete_block_list_docs},
    {"get_block_content", get_block_content, METH_VARARGS, get_block_content_docs},
    {"bwsiter", bora_bwiter, METH_O, bwiter_docs},
    {"bws_set", bora_bws_set, METH_VARARGS, bws_set_docs},
    {"send_bw_msg", bora_send_bw_msg, METH_VARARGS, send_bw_msg_docs},
    {"get_bw_msg", bora_get_bw_msg, METH_NOARGS, get_bw_msg_docs},
    {"bora_send_cookie", bora_send_cookie, METH_VARARGS, send_cookie_docs},
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

  bora_BWIterType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&bora_BWIterType) < 0)  return;

  #ifdef __WIN32__
  init_wsa();
  #endif

  m = Py_InitModule("bora", BoraMethods);

  Py_INCREF(&bora_BIterType);
  PyModule_AddObject(m, "_BIter", (PyObject *)&bora_BIterType);

  Py_INCREF(&bora_BPullerType);
  PyModule_AddObject(m, "_BPuller", (PyObject *)&bora_BPullerType);

  Py_INCREF(&bora_BWIterType);
  PyModule_AddObject(m, "_BWIter", (PyObject *)&bora_BWIterType);
}
