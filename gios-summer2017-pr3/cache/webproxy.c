#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>

#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>

#include "gfserver.h"

#include "shm_channel.h"

/* note that the -n and -z parameters are NOT used for Part 1 */
/* they are only used for Part 2 */                         
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 2)\n"                      \
"  -p [listen_port]    Listen port (Default: 8140)\n"                                 \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1024)\n"              \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"     \
"  -z [segment_size]   The segment size (in bytes, Default: 256).\n"                  \
"  -h                  Show this help message\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"segment-count", required_argument,      NULL,           'n'},
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},
  {"segment-size",  required_argument,      NULL,           'z'},         
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,            0}
};

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg, int shmid, int segment_size);

static gfserver_t gfs;

// Queue for managing free segments
steque_t* free_segment_queue;
pthread_mutex_t free_segment_queue_mutex; // for free_segment_queue
pthread_cond_t free_segment_queue_cv;

int* shmid_list;
int shmid_list_len;
size_t segment_size;

static void _sig_handler(int signo){
  int i, shmid;
  key_t segment_key;

  if (signo == SIGINT || signo == SIGTERM){
    gfserver_stop(&gfs);

    // Shared memory cleanup
    for (i = 0; i < shmid_list_len; i++) {
      if ((segment_key = ftok(SHARED_MEMORY_NAME, i)) < 0) {
        perror("Error creating segment_key");
        exit(1);
      }
      shmid = shmget(segment_key, segment_size, 0666 | IPC_CREAT);
      if ((shmctl(shmid, IPC_RMID, NULL)) < 0) {
        perror("Error removing shared memory\n");
      }
    }

    steque_destroy(free_segment_queue);
    free(shmid_list);

    // Message queue cleanup
    key_t req_key, res_key, rereq_key;
    int req_msqid, res_msqid, rereq_msqid;

    req_key = ftok(REQ_QUEUE_NAME, 0);
    res_key = ftok(RES_QUEUE_NAME, 0);
    rereq_key = ftok(RES_QUEUE_NAME, 0);

    req_msqid = msgget(req_key, 0666 | IPC_CREAT);
    res_msqid = msgget(res_key, 0666 | IPC_CREAT);
    rereq_msqid = msgget(rereq_key, 0666 | IPC_CREAT);

    msgctl(req_msqid, IPC_RMID, NULL);
    msgctl(res_msqid, IPC_RMID, NULL);
    msgctl(rereq_msqid, IPC_RMID, NULL);

    exit(signo);
  }
}

ssize_t client_request_handler(gfcontext_t *ctx, char *path, void* arg) {
  int segid;
  ssize_t result;

  // Get free segment
  pthread_mutex_lock(&free_segment_queue_mutex);

  while((steque_size(free_segment_queue)) < 1) {
    pthread_cond_wait(&free_segment_queue_cv, &free_segment_queue_mutex);
  }

  segid = *((int*)steque_pop(free_segment_queue));
  printf("Segment -- %d: used\n", segid);

  pthread_mutex_unlock(&free_segment_queue_mutex);
  pthread_cond_broadcast(&free_segment_queue_cv);

  // handle_with_cache
  result = handle_with_cache(ctx, path, arg, segid, segment_size);

  // Once handle_with_cache returns, return the segment into free segment queue
  pthread_mutex_lock(&free_segment_queue_mutex);

  steque_enqueue(free_segment_queue, (steque_item)(&shmid_list[segid]));
  printf("Segment -- %d: freed\n", segid);

  pthread_mutex_unlock(&free_segment_queue_mutex);
  pthread_cond_broadcast(&free_segment_queue_cv);

  return result;
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int i;
  int option_char = 0;
  unsigned short port = 8140;
  unsigned short nworkerthreads = 1;
  unsigned int nsegments = 2;
  size_t segsize = 256;
  char *server = "s3.amazonaws.com/content.udacity-data.com";

  /* disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGINT, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  /* Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "n:p:s:t:z:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;   
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;                                          
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(__LINE__);
    }
  }

  if (!server) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(__LINE__);
  }

  if (segsize < 128) {
    fprintf(stderr, "Invalid segment size\n");
    exit(__LINE__);
  }

  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(__LINE__);
  }

 if (port < 1024) {
    fprintf(stderr, "Invalid port number\n");
    exit(__LINE__);
  }

  if ((nworkerthreads < 1) || (nworkerthreads > 1024)) {
    fprintf(stderr, "Invalid number of worker threads\n");
    exit(__LINE__);
  }


  /* This is where you initialize your shared memory */
  int j;
  key_t segment_key;

  shmid_list = malloc(sizeof(int) * nsegments);
  shmid_list_len = nsegments;
  segment_size = segsize;

  free_segment_queue = (steque_t*) malloc(sizeof(steque_t));
  steque_init(free_segment_queue);

  pthread_mutex_lock(&free_segment_queue_mutex);

  for (j = 0; j < nsegments; j++) {
    if ((segment_key = ftok(SHARED_MEMORY_NAME, j)) < 0) {
      printf("Error creating segment key for segid: %d\n", j);
      exit(__LINE__);
    }
    if ((shmget(segment_key, segment_size, 0666 | IPC_CREAT)) < 0) {
      printf("Error connecting to segment for segid: %d\n", j);
      exit(__LINE__);
    }

    shmid_list[j] = j;

    steque_enqueue(free_segment_queue, (steque_item)(&shmid_list[j]));
  }

  pthread_mutex_unlock(&free_segment_queue_mutex);

  /* This is where you initialize the server struct */
  gfserver_init(&gfs, nworkerthreads);

  /* This is where you set the options for the server */
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 12);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, client_request_handler);
  for(i = 0; i < nworkerthreads; i++) {
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, "data");
  }
  
  /* This is where you invoke the framework to run the server */
  /* Note that it loops forever */
  gfserver_serve(&gfs);
}
