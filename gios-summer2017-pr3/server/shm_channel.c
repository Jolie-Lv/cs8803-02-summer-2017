#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include "shm_channel.h"
#include "gfserver.h"

//In case you want to implement the shared memory IPC as a library...

ssize_t from_cache_to_shared_memory(int res_msqid, ResMsgBuf* res_msgbuf, int req_msqid, int fildes, int segid, size_t segment_size) {
  size_t file_len, bytes_transferred = 0;
  ssize_t read_len;
  char *buffer;
  char *shm_pointer;
  int shmid;
  key_t segment_key;
  ReqMsgBuf req_msgbuf;

  buffer = (char*)malloc(segment_size * sizeof(char));

  if ((segment_key = ftok(SHARED_MEMORY_NAME, segid)) < 0) {
    perror("Error creating segment_key\n");
    return -1;
  }
  if ((shmid = shmget(segment_key, segment_size * sizeof(char), 0666 | IPC_CREAT)) < 0) {
    perror("Error connecting to segment\n");
    return -1;
  }
  shm_pointer = (char*)shmat(shmid, (void *)0, 0);
  if (shm_pointer == (char *)(-1)) {
    perror("Error getting pointer\n");
    return -1;
  }

  file_len = lseek(fildes, 0, SEEK_END);
  lseek(fildes, 0, SEEK_SET);

  do {
    memset(shm_pointer, '\0', segment_size);
    memset(buffer, '\0', segment_size);

    read_len = read(fildes, buffer, segment_size);
    if (read_len <= 0) {
      perror("Error from reading cached file\n");
      return -1;
    }

    memcpy(shm_pointer, buffer, read_len);

    bytes_transferred += read_len;

    res_msgbuf->message.status = 200;
    res_msgbuf->message.response_size = read_len;
    res_msgbuf->message.bytes_transferred = bytes_transferred;
    res_msgbuf->message.file_size = file_len;

    // Send res_msgbuf to response queue
    if (msgsnd(res_msqid, res_msgbuf, sizeof(res_msgbuf->message), 0) < 0) {
      perror("Error sending message from cache to res_message_queue\n");
      return -1;
    }

    printf("Progress for segid: %d -- bytes_transferred: %zu -- total length: %zu\n", (int)res_msgbuf->mtype,
           bytes_transferred, file_len);

    if (bytes_transferred < file_len) {
      if (msgrcv(req_msqid, &req_msgbuf, sizeof(req_msgbuf.req_message), res_msgbuf->mtype, 0) < 0) {
        perror("Error receiving message from req_message_queue to cache\n");
        return -1;
      }
    }
  } while(bytes_transferred < file_len);


  shmdt(shm_pointer);
  free(buffer);

  return bytes_transferred;
}

/*
ssize_t from_shared_memory_to_client(gfcontext_t *ctx, int shmid, int seg_length) {
  ssize_t write_len;
  char* shm_pointer;

  shm_pointer = shmat(shmid, (void *)0, 0);

  write_len = gfs_send(ctx, shm_pointer, seg_length);
  if (write_len != seg_length){
    printf("Error in sending client segment\n");
    return -1;
  }

  return write_len;
}
*/
