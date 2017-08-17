#include "minifyjpeg.h"
#include "magickminify.h"

#include <unistd.h>

/* Implement the needed server-side functions here */
extern  minifyjpeg_res * minifyjpeg_proc_1_svc(minify_in* rpc_in, struct svc_req* request) {
  ssize_t minifyjpeg_len;
  void* minifyjpeg_img;
  minify_out rpc_out;
  static minifyjpeg_res res;

  xdr_free((xdrproc_t)xdr_minifyjpeg_res, (char*)&res);

  magickminify_init();

  minifyjpeg_img = magickminify(rpc_in->high_res_img.high_res_img_val, rpc_in->high_res_img.high_res_img_len,
    &minifyjpeg_len);

  rpc_out.low_res_img.low_res_img_val = minifyjpeg_img;
  rpc_out.low_res_img.low_res_img_len = (unsigned int)minifyjpeg_len;

  magickminify_cleanup();

  res.minifyjpeg_res_u.res_img = rpc_out;

  return &res;
}