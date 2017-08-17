/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */

 struct minify_in {
  opaque high_res_img<>;
 };
 struct minify_out {
  opaque low_res_img<>;
 };

 program MINIFYJPEG_PROG { /* RPC service name */
   version MINIFYJPEG_VERS {
     minify_out MINIFYJPEG_PROC(minify_in) = 1; /* proc1 */
   } = 1; /* version1 */
 } = 0x31230000; /* service id */