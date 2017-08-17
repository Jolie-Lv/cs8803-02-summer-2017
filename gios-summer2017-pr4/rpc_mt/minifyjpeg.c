#include "minifyjpeg.h"
#include "magickminify.h"

/* Implement the needed server-side functions here */
extern  bool_t minifyjpeg_proc_1_svc(minify_in rpc_in, minify_out* rpc_out, struct svc_req* request) {
  ssize_t minifyjpeg_len;
  void* minifyjpeg_img;

  magickminify_init();

  minifyjpeg_img = magickminify(rpc_in.high_res_img.high_res_img_val, rpc_in.high_res_img.high_res_img_len,
                                &minifyjpeg_len);

  rpc_out->low_res_img.low_res_img_val = minifyjpeg_img;
  rpc_out->low_res_img.low_res_img_len = (unsigned int)minifyjpeg_len;

  magickminify_cleanup();

  return TRUE;
}

extern int minifyjpeg_prog_1_freeresult (SVCXPRT *transp, xdrproc_t _xdr_result, caddr_t result) {
  xdr_free(_xdr_result, result);

  return 1;
}
