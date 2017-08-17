#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>

#include "gfserver.h"
#include "content.h"

#include <pthread.h>
#include "steque.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  gfserver_main [options]\n"                                                 \
"options:\n"                                                                  \
"  -t [nthreads]       Number of threads (Default: 2)\n"                      \
"  -p [listen_port]    Listen port (Default: 8140)\n"                         \
"  -c [content_file]   Content file mapping keys to content files\n"          \
"  -h                  Show this help message.\n"                             \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"port",          required_argument,      NULL,           'p'},
        {"nthreads",      required_argument,      NULL,           't'},
        {"content",       required_argument,      NULL,           'c'},
        {"help",          no_argument,            NULL,           'h'},
        {NULL,            0,                      NULL,             0}
};


extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);

static void _sig_handler(int signo){
    if (signo == SIGINT || signo == SIGTERM){
        exit(signo);
    }
}

/* Global variables for thread and task management */
pthread_t *worker_threads;
typedef struct ClientRequest {
    gfcontext_t* ctx;
    char* path;
    void* arg;
} ClientRequest;
steque_t* client_request_queue;
pthread_mutex_t request_queue_mutex; // for client_request_queue
pthread_cond_t request_queue_cv;


/* Queue client request */
void queue_client_request(ClientRequest* client_request) {
    pthread_mutex_lock(&request_queue_mutex);

    steque_enqueue(client_request_queue, (steque_item)client_request);

    printf("[Main] -- enqueue request: %s\n", client_request->path);

    pthread_mutex_unlock(&request_queue_mutex);

    // Signal workers that there is a new task queued
    pthread_cond_broadcast(&request_queue_cv);
}

/* Client request handler */
ssize_t client_request_handler(gfcontext_t *ctx, char *path, void *arg) {
    ClientRequest* client_request = (ClientRequest*)malloc(sizeof(ClientRequest));

    client_request->ctx = ctx;
    client_request->path = path;
    client_request->arg = arg;

    queue_client_request(client_request);

    return 0;
}

/* Work thread task function */
void *assign_worker_to_task(void *arg) {
    long thread_id;

    thread_id = (long)arg;

    printf("[Worker thread #%ld] -- created\n", thread_id);

    for(;;) {
        pthread_mutex_lock(&request_queue_mutex);

        while(steque_size(client_request_queue) == 0) {
            printf("[Worker thread #%ld] -- waiting\n", thread_id);
            pthread_cond_wait(&request_queue_cv, &request_queue_mutex);
        }

        ClientRequest* request = (ClientRequest*)steque_pop(client_request_queue);
        pthread_mutex_unlock(&request_queue_mutex);

        printf("[Worker thread #%ld] -- received request_path: %s\n", thread_id, request->path);

        if (handler_get(request->ctx, request->path, request->arg) < 0) {
            printf("[Worker thread #%ld] -- error during handling request\n", thread_id);
        }

        free(request);
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
    int option_char = 0;
    unsigned short port = 8140;
    char *content = "content.txt";
    gfserver_t *gfs = NULL;
    int nthreads = 2;
    long i;
    int rc;

    if (signal(SIGINT, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGINT...exiting.\n");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, _sig_handler) == SIG_ERR){
        fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
        exit(EXIT_FAILURE);
    }

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:t:hc:", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            default:
                fprintf(stderr, "%s", USAGE);
                exit(1);
            case 'p': // listen-port
                port = atoi(optarg);
                break;
            case 't': // nthreads
                nthreads = atoi(optarg);
                break;
            case 'h': // help
                fprintf(stdout, "%s", USAGE);
                exit(0);
                break;
            case 'c': // file-path
                content = optarg;
                break;
        }
    }

    /* not useful, but it ensures the initial code builds without warnings */
    if (nthreads < 1) {
        nthreads = 1;
    }
    content_init(content);

    // Initialize global variables related to worker and task queue management
    pthread_mutex_init(&request_queue_mutex, NULL);
    pthread_cond_init(&request_queue_cv, NULL);
    client_request_queue = (steque_t*) malloc(sizeof(steque_t));
    steque_init(client_request_queue);

    /*Initializing server*/
    gfs = gfserver_create();

    /*Setting options*/
    gfserver_set_port(gfs, port);
    gfserver_set_maxpending(gfs, 64);
    gfserver_set_handler(gfs, client_request_handler);
    gfserver_set_handlerarg(gfs, NULL);

    /* Initialize worker threads */
    worker_threads = malloc(sizeof(pthread_t) * nthreads);
    for (i = 0; i < nthreads; i++) {
        rc = pthread_create(&worker_threads[i], NULL, assign_worker_to_task, (void*)i);
        if (rc) {
            perror("Error during work thread initialization");
            exit(1);
        }
    }

    /*Loops forever*/
    gfserver_serve(gfs);
}
