/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */

 struct minify_in {
  opaque high_res_img<>;
 };
 struct minify_out {
  opaque low_res_img<>;
 };

 union minifyjpeg_res switch (int errno) {
    case 0:
      minify_out res_img;
    default:
      void;
    };

 program MINIFYJPEG_PROG { /* RPC service name */
   version MINIFYJPEG_VERS {
     minifyjpeg_res MINIFYJPEG_PROC(minify_in) = 1; /* proc1 */
   } = 1; /* version1 */
 } = 0x31230000; /* service id */