#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "shm_channel.h"
#include "simplecache.h"


#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE


static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/

		simplecache_destroy();
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1024)\n"      \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {"nthreads",           required_argument,      NULL,           't'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

// Thread management
pthread_t *worker_threads;
steque_t* client_request_queue;
pthread_mutex_t request_queue_mutex; // for client_request_queue
pthread_cond_t request_queue_cv;

int req_msqid, res_msqid, rereq_msqid;

typedef struct ProxyRequest {
	ReqMsgBuf req_msgbuf;
} ProxyRequest;

void *assign_worker_to_task(void *arg) {
	long thread_id;
	int cached_fildes;

	ResMsgBuf res_msgbuf;

	thread_id = (long)arg;

	printf("[Worker thread #%ld] -- created\n", thread_id);

	for(;;) {
		pthread_mutex_lock(&request_queue_mutex);

		while(steque_size(client_request_queue) == 0) {
			printf("[Worker thread #%ld] -- waiting\n", thread_id);
			pthread_cond_wait(&request_queue_cv, &request_queue_mutex);
		}

		ProxyRequest* proxy_request = (ProxyRequest*)steque_pop(client_request_queue);
		pthread_mutex_unlock(&request_queue_mutex);

		printf("[Worker thread #%ld] -- received request_path: %s\n", thread_id, proxy_request->req_msgbuf.req_message.path);

		// Check if request exist in cache
		cached_fildes = simplecache_get(proxy_request->req_msgbuf.req_message.path);

		// Formulate res_msgbuf
		res_msgbuf.mtype = proxy_request->req_msgbuf.mtype;
		if (cached_fildes < 0) {
			res_msgbuf.message.status = 400;
		}
		else {
			if ((from_cache_to_shared_memory(res_msqid, &res_msgbuf, req_msqid, rereq_msqid, cached_fildes,
																			 proxy_request->req_msgbuf.req_message.segid,
																			 proxy_request->req_msgbuf.req_message.segment_size)) < 0) {
				res_msgbuf.message.status = 500;
			}
			else {
				printf("Response for path: %s -- size: %zu -- segid: %d\n",
							 proxy_request->req_msgbuf.req_message.path,
							 res_msgbuf.message.response_size,
							 proxy_request->req_msgbuf.req_message.segid);

				continue;
			}
		}

		// Send res_msgbuf to response queue
		if (msgsnd(res_msqid, &res_msgbuf, sizeof(res_msgbuf.message), 0) < 0) {
			perror("Error sending message from cache to res_message_queue\n");
			return 0;
		}

		free(proxy_request);
	}
}

int main(int argc, char **argv) {
	int nthreads = 1, rc;
	char *cachedir = "locals.txt";
	char option_char;
  long i;

	key_t req_key, res_key, rereq_key;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "c:ht:", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			default:
				Usage();
				exit(1);
		}
	}

	if ((nthreads>1024) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads\n");
		exit(__LINE__);
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	/* Cache initialization */
	simplecache_init(cachedir);

	/* Add your cache code here */

	// Connect to message queue
	req_key = ftok(REQ_QUEUE_NAME, 0);
	res_key = ftok(RES_QUEUE_NAME, 0);
	rereq_key = ftok(REREQ_QUEUE_NAME, 0);

	req_msqid = msgget(req_key, 0666 | IPC_CREAT);
	res_msqid = msgget(res_key, 0666 | IPC_CREAT);
	rereq_msqid = msgget(rereq_key, 0666 | IPC_CREAT);

	// Initialize global variables related to worker and task queue management
	pthread_mutex_init(&request_queue_mutex, NULL);
	pthread_cond_init(&request_queue_cv, NULL);
	client_request_queue = (steque_t*) malloc(sizeof(steque_t));
	steque_init(client_request_queue);

  /* Initialize worker threads */
  worker_threads = malloc(sizeof(pthread_t) * nthreads);
  for (i = 0; i < nthreads; i++) {
    rc = pthread_create(&worker_threads[i], NULL, assign_worker_to_task, (void*)i);
    if (rc) {
      perror("Error during work thread initialization");
      exit(1);
    }
  }

  for(;;) {
		ProxyRequest* proxy_request = (ProxyRequest*)malloc(sizeof(ProxyRequest));

    // Read request message queue
		if (msgrcv(req_msqid, &(proxy_request->req_msgbuf), sizeof(proxy_request->req_msgbuf.req_message), 0, 0) < 0) {
			perror("Error receiving message from req_message_queue to cache\n");
			return 0;
		}

    printf("Received request for path: %s -- segid: %d\n", proxy_request->req_msgbuf.req_message.path,
					 proxy_request->req_msgbuf.req_message.segid);

		pthread_mutex_lock(&request_queue_mutex);

		steque_enqueue(client_request_queue, (steque_item)proxy_request);

		printf("[Main] -- enqueue request: %s\n", proxy_request->req_msgbuf.req_message.path);

		pthread_mutex_unlock(&request_queue_mutex);

		// Signal workers that there is a new task queued
		pthread_cond_broadcast(&request_queue_cv);
  }

	/* this code probably won't execute */
	return 0;
}
