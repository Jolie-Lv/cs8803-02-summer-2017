#include <stdlib.h>
#include <stdio.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"

#include "minifyjpeg.h"


CLIENT* get_minify_client(char *server){
  CLIENT *cl;
  struct timeval tv;

  if ((cl = clnt_create(server, MINIFYJPEG_PROG, MINIFYJPEG_VERS, "tcp")) == NULL) {
    clnt_pcreateerror(server);
    exit(1);
  }

  tv.tv_sec = 5;	/* change time-out to 5 seconds	*/
  tv.tv_usec = 0;
  clnt_control(cl, CLSET_TIMEOUT, (char*)&tv);

  return cl;
}


void* minify_via_rpc(CLIENT *cl, void* src_val, size_t src_len, size_t *dst_len){
  minifyjpeg_res* res;
  minify_out rpc_out;
  minify_in request_args;

  request_args.high_res_img.high_res_img_len = (unsigned int)src_len;
  request_args.high_res_img.high_res_img_val = src_val;

  res = minifyjpeg_proc_1(&request_args, cl);
  if (res == NULL) {
    clnt_perror(cl, (char*)src_val);

    free(src_val);

    exit(124);
  }
  rpc_out = res->minifyjpeg_res_u.res_img;
  *dst_len = rpc_out.low_res_img.low_res_img_len;

  return rpc_out.low_res_img.low_res_img_val;
}