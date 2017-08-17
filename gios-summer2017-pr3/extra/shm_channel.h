#ifndef _SHM_CHANNEL_H_
#define _SHM_CHANNEL_H_

#include "gfserver.h"

#define REQ_QUEUE_NAME "handle_with_cache.c"
#define RES_QUEUE_NAME "simplecached.c"
#define REREQ_QUEUE_NAME "simplecached.h"
#define SHARED_MEMORY_NAME "shm_channel.h"

#define MAX_CACHE_REQUEST_LEN 1024

//In case you want to implement the shared memory IPC as a library...

typedef struct ReqMsgbuf {
  long mtype;
  struct req_message {
    char path[MAX_CACHE_REQUEST_LEN];
    int segid;
    size_t segment_size;
  } req_message;
} ReqMsgBuf;

typedef struct ResMsgBuf {
  long mtype;
  struct message {
    int status;
    size_t response_size;
    size_t bytes_transferred;
    size_t file_size;
  } message;
} ResMsgBuf;

typedef struct ResMsgBuf ResMsgBuf;

ssize_t from_cache_to_shared_memory(int res_msqid, ResMsgBuf* res_msgbuf, int req_msqid, int rereq_msqid,
                                    int fildes, int shmid, size_t segment_size);

// ssize_t from_shared_memory_to_client(gfcontext_t *ctx, int shmid, int seg_length);

#endif