#include <stdlib.h>
#include <stdio.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"

CLIENT* get_minify_client(char *server){
    CLIENT *cl;
    struct timeval tv;

    if ((cl = clnt_create(server, MINIFYJPEG_PROG, MINIFYJPEG_VERS, "tcp")) == NULL) {
        clnt_pcreateerror(server);
        exit(1);
    }

    tv.tv_sec = 100;	/* change time-out to 5 seconds	*/
    tv.tv_usec = 0;
    clnt_control(cl, CLSET_TIMEOUT, (char*)&tv);

    return cl;
}


void* minify_via_rpc(CLIENT *cl, void* src_val, size_t src_len, size_t *dst_len){
    minify_out rpc_out;
    minify_in request_args;
    enum clnt_stat clnt_stat;

    request_args.high_res_img.high_res_img_len = (unsigned int)src_len;
    request_args.high_res_img.high_res_img_val = src_val;


    clnt_stat = minifyjpeg_proc_1(request_args, &rpc_out, cl);
    if (clnt_stat == RPC_TIMEDOUT) {
        clnt_perror(cl, (char*)src_val);

        exit(124);
    }
    else if (clnt_stat != RPC_SUCCESS) {
        clnt_perror(cl, (char*)src_val);

        exit(1);
    }
    *dst_len = rpc_out.low_res_img.low_res_img_len;

    printf("received");

    return rpc_out.low_res_img.low_res_img_val;
}