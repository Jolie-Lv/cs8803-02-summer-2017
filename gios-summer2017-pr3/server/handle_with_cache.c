#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "gfserver.h"
#include "shm_channel.h"

#define MAX_CACHE_REQUEST_LEN 1024

//Replace with an implementation of handle_with_cache and any other
//functions you may need.

void handle_error(int pid, char* path, int segid, char* message, int response_status, gfcontext_t* ctx) {
  printf("[pid: %d, path: %s, segid: %d] ERROR\n", pid, path, segid);
  perror(message);

  gfs_sendheader(ctx, response_status, 0);
}

// Connect to res and req message queues
void initialize_message_queue(int *req_msqid, int *res_msqid, int *rereq_msqid) {
  key_t req_key, res_key, rereq_key;

  req_key = ftok(REQ_QUEUE_NAME, 0);
  res_key = ftok(RES_QUEUE_NAME, 0);
  rereq_key = ftok(REREQ_QUEUE_NAME, 0);

  *req_msqid = msgget(req_key, 0666 | IPC_CREAT);
  *res_msqid = msgget(res_key, 0666 | IPC_CREAT);
  *rereq_msqid = msgget(rereq_key, 0666 | IPC_CREAT);
}

// Connect to shared memory
char* connect_to_shared_memory(int segid, int *shmid, size_t segment_size, int pid, char* path, gfcontext_t* ctx) {
  key_t segment_key;
  char* shm_pointer;

  if ((segment_key = ftok(SHARED_MEMORY_NAME, segid)) < 0) {
    handle_error(pid, path, segid, "Error creating segment key\n", GF_ERROR, ctx);
    return (char*)-1;
  }
  if ((*shmid = shmget(segment_key, segment_size, 0666)) < 0) {
    handle_error(pid, path, segid, "Error connecting to segment\n", GF_ERROR, ctx);
    return (char*)-1;
  }
  shm_pointer = shmat(*shmid, (void *)0, 0);
  if (shm_pointer == (char *)(-1)) {
    handle_error(pid, path, segid, "Error getting pointer\n", GF_ERROR, ctx);
    return (char*)-1;
  }

  return shm_pointer;
}

// Construct message and send to queue
int send_message_to_req_queue(ReqMsgBuf *req_msgbuf, int req_msqid, int pid, char *path, int segid, size_t segment_size) {
  req_msgbuf->mtype = pid;
  strcpy(req_msgbuf->req_message.path, path);
  req_msgbuf->req_message.segid = segid;
  req_msgbuf->req_message.segment_size = segment_size;

  return msgsnd(req_msqid, req_msgbuf, sizeof(req_msgbuf->req_message), 0);
}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg, int segid, size_t segment_size){
  int pid, req_msqid, res_msqid, rereq_msqid;
  ReqMsgBuf req_msgbuf;
  ResMsgBuf res_msgbuf;
  // ssize_t byte_transferred;

  initialize_message_queue(&req_msqid, &res_msqid, &rereq_msqid);

  pid = (int)getpid();

  if (send_message_to_req_queue(&req_msgbuf, req_msqid, pid, path, segid, segment_size) < 0) {
    handle_error(pid, path, segid, "Error sending message to req_msqid\n", GF_ERROR, ctx);
    return -1;
  }

  printf("[pid: %d, path: %s, segid: %d] Send request to req_msqid\n", pid, path, segid);

  if (msgrcv(res_msqid, &res_msgbuf, sizeof(res_msgbuf.message), pid, 0) < 0) {
    handle_error(pid, path, segid, "Error receiving message from res_msqid\n", GF_ERROR, ctx);
    return -1;
  }

  printf("[pid: %d, path: %s, segid: %d] Received response with status: %d \n",
         pid, path, segid, res_msgbuf.message.status);

  if (res_msgbuf.message.status != GF_OK) {
    gfs_sendheader(ctx, res_msgbuf.message.status, 0);
    printf("[pid: %d, path: %s, segid: %d] Send error header\n",
           pid, path, segid);

    return -1;
  }
  else {
    ssize_t write_len;
    char *shm_pointer;
    size_t response_size, file_size, remaining_message_size;
    int shmid;

    response_size = res_msgbuf.message.response_size;
    file_size = res_msgbuf.message.file_size;

    printf("[pid: %d, path: %s, segid: %d] Read shared memory of size: %zu\n", pid, path, segid, response_size);

    shm_pointer = connect_to_shared_memory(segid, &shmid, segment_size, pid, path, ctx);
    if (shm_pointer == (char *)(-1)) {
      return -1;
    }
    else {
      gfs_sendheader(ctx, GF_OK, file_size);
    }

    remaining_message_size = file_size;

    do {
      write_len = gfs_send(ctx, shm_pointer, response_size);
      if (write_len != response_size){
        handle_error(pid, path, segid, "Error sending client the segment data\n", GF_ERROR, ctx);
        return -1;
      }
      remaining_message_size -= write_len;

      /*
      printf("[pid: %d, path: %s, segid: %d] Response sent to client of size: %zu\n", pid, path, segid, write_len);
      printf("[pid: %d, path: %s, segid: %d] Byte transferred so far: %zu\n", pid, path, segid, res_msgbuf.message.bytes_transferred);
      printf("[pid: %d, path: %s, segid: %d] Reamining message: %zu\n", pid, path, segid, remaining_message_size);
       */

      if (res_msgbuf.message.bytes_transferred < file_size) {
        // Send res_msgbuf to response queue
        if (msgsnd(rereq_msqid, &req_msgbuf, sizeof(req_msgbuf.req_message), 0) < 0) {
          perror("Error sending message from cache to res_message_queue\n");
          gfs_sendheader(ctx, GF_ERROR, 0);
          return -1;
        }

        if (msgrcv(res_msqid, &res_msgbuf, sizeof(res_msgbuf.message), pid, 0) < 0) {
          perror("Error receiving message from req_message_queue to cache\n");
          gfs_sendheader(ctx, GF_ERROR, 0);
          return -1;
        }

        response_size = res_msgbuf.message.response_size;
      }
    } while(remaining_message_size > 0);

    shmdt(shm_pointer);

    return res_msgbuf.message.bytes_transferred;
  }
}

