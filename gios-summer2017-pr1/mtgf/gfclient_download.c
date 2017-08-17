#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>

#include "gfclient.h"
#include "workload.h"

#include <pthread.h>
#include "steque.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -n [num_requests]   Requests download per thread (Default: 2)\n"           \
"  -p [server_port]    Server port (Default: 8140)\n"                         \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -t [nthreads]       Number of threads (Default 2)\n"                       \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
        {"help",          no_argument,            NULL,           'h'},
        {"nthreads",      required_argument,      NULL,           't'},
        {"nrequests",     required_argument,      NULL,           'n'},
        {"server",        required_argument,      NULL,           's'},
        {"port",          required_argument,      NULL,           'p'},
        {"workload-path", required_argument,      NULL,           'w'},
        {NULL,            0,                      NULL,             0}
};

static void Usage() {
    fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
    static int counter = 0;

    sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
    char *cur, *prev;
    FILE *ans;

    /* Make the directory if it isn't there */
    prev = path;
    while(NULL != (cur = strchr(prev+1, '/'))){
        *cur = '\0';

        if (0 > mkdir(&path[0], S_IRWXU)){
            if (errno != EEXIST){
                perror("Unable to create directory");
                exit(EXIT_FAILURE);
            }
        }

        *cur = '/';
        prev = cur;
    }

    if( NULL == (ans = fopen(&path[0], "w"))){
        perror("Unable to open file");
        exit(EXIT_FAILURE);
    }

    return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
    FILE *file = (FILE*) arg;

    fwrite(data, 1, data_len, file);
}

// Global variable for server initialization
static char *server = "localhost";
static unsigned short port = 8140;

// Global variables for worker thread and task queue management
static steque_t* request_queue;
static pthread_mutex_t request_queue_mutex; // for client_request_queue
static pthread_cond_t request_queue_cv;
static int queuing_request_finished = 0;

static void make_request_to_server(char* request_path, long thread_id) {
    int returncode = 0;
    char local_path[512];
    FILE *file = NULL;
    gfcrequest_t *gfr = NULL;

    localPath(request_path, local_path);

    file = openFile(local_path);

    // Initialize variables for this thread
    gfr = gfc_create();
    gfc_set_server(gfr, server);
    gfc_set_port(gfr, port);
    gfc_set_writefunc(gfr, writecb);
    gfc_set_path(gfr, request_path);
    gfc_set_writearg(gfr, file);

    printf("[Worker thread #%ld] -- Requesting %s%s\n", thread_id, server, request_path);

    if ( 0 > (returncode = gfc_perform(gfr))){
        printf("[Worker thread #%ld]gfc_perform returned an error %d\n", thread_id, returncode);
        fclose(file);
        if ( 0 > unlink(local_path))
            printf("[Worker thread #%ld]unlink failed on %s\n", thread_id, local_path);
    }
    else {
        fclose(file);
    }

    if ( gfc_get_status(gfr) != GF_OK){
        if ( 0 > unlink(local_path))
            printf("[Worker thread #%ld]unlink failed on %s\n", thread_id, local_path);
    }

    printf("[Worker thread #%ld]Status: %s\n", thread_id, gfc_strstatus(gfc_get_status(gfr)));
    printf("[Worker thread #%ld]Received %zu of %zu bytes\n", thread_id, gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));

    // Clean up
    gfc_cleanup(gfr);
}

/* Work thread task function */
static void *assign_worker_to_task(void *arg) {
    long thread_id;
    char* request_path;

    thread_id = (long)arg;

    printf("[Worker thread #%ld] -- created\n", thread_id);

    // Perform request if there is an item in queue
    for (;;) {
        pthread_mutex_lock(&request_queue_mutex);

        // Wait while queue is empty AND queuing request from main thread ins't finished
        while(steque_size(request_queue) == 0 && queuing_request_finished == 0) {
            printf("[Worker thread #%ld] -- waiting\n", thread_id);
            pthread_cond_wait(&request_queue_cv, &request_queue_mutex);
        }

        // Exit if queue is empty and main thread finished queuing up requests
        if (steque_size(request_queue) == 0 && queuing_request_finished != 0) {
            printf("[Worker thread #%ld] -- exiting\n", thread_id);
            pthread_mutex_unlock(&request_queue_mutex);

            return 0;
        }
        else {
            // Perform request
            request_path = (char*)steque_pop(request_queue);
            pthread_mutex_unlock(&request_queue_mutex);

            make_request_to_server(request_path, thread_id);
        }
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
    char *workload_path = "workload.txt";

    int i = 0;
    int option_char = 0;
    int nrequests = 2;
    int nthreads = 2;
    char *req_path = NULL;

    // Variables for worker thread management
    int rc;
    long j;
    pthread_t* worker_threads;
    void *status;

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "hn:p:s:t:w:", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            case 'h': // help
                Usage();
                exit(0);
                break;
            case 'n': // nrequests
                nrequests = atoi(optarg);
                break;
            case 'p': // port
                port = atoi(optarg);
                break;
            case 's': // server
                server = optarg;
                break;
            case 't': // nthreads
                nthreads = atoi(optarg);
                break;
            case 'w': // workload-path
                workload_path = optarg;
                break;
            default:
                Usage();
                exit(1);
        }
    }

    if( EXIT_SUCCESS != workload_init(workload_path)){
        fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
        exit(EXIT_FAILURE);
    }

    // Initialize global variables related to worker and task queue management
    gfc_global_init();

    pthread_mutex_init(&request_queue_mutex, NULL);
    pthread_cond_init(&request_queue_cv, NULL);
    request_queue = (steque_t*) malloc(sizeof(steque_t));
    steque_init(request_queue);

    /* Initialize worker threads */
    worker_threads = malloc(sizeof(pthread_t) * nthreads);
    for (j = 0; j < nthreads; j++) {
        rc = pthread_create(&worker_threads[j], NULL, assign_worker_to_task, (void*)j);
        if (rc) {
            perror("Error during work thread initialization");
            exit(1);
        }
    }

    /*Making the requests...*/
    for(i = 0; i < nrequests * nthreads; i++){
        req_path = workload_get_path();

        if(strlen(req_path) > 256){
            fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
            exit(EXIT_FAILURE);
        }

        // Queue req_path to request_queue
        pthread_mutex_lock(&request_queue_mutex);

        steque_enqueue(request_queue, (void*)req_path);

        pthread_mutex_unlock(&request_queue_mutex);

        pthread_cond_broadcast(&request_queue_cv);
    }

    // Indicate queuing request is finished
    pthread_mutex_lock(&request_queue_mutex);
    queuing_request_finished = 1;
    pthread_cond_broadcast(&request_queue_cv); // So that workers who already went to sleep don't get stuck there
    pthread_mutex_unlock(&request_queue_mutex);

    // Wait for worker threads to join
    for (j = 0; j < nthreads; j++) {
        rc = pthread_join(worker_threads[j], &status);
        if (rc) {
            perror("Error during work thread initialization");
            exit(1);
        }

        printf("[Main thread] Worker thread #%ld returned status: \n", j);
    }

    gfc_global_cleanup();

    // Cleanup global variables related to worker and task queue management
    pthread_mutex_destroy(&request_queue_mutex);
    pthread_cond_destroy(&request_queue_cv);
    steque_destroy(request_queue);
    free(request_queue);
    free(worker_threads);

    return 0;
}  
