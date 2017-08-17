# Project README file

Author: Jongho Jung (jjung327)

## Project Description

### Main Part

**Overall Approach**

First, XDR file was written, and with `rpcgen -C`, codes are auto-generated. 

On client-side, two functions in `minify_via_rpc.c` were implemented. 

On server-side, `minifyjpeg.c`, which contains actual function that low-sizes an image. 

**Design Details**

Since an high quality image and down-sized image needs to be transferred between client and server,
opaque array was used for image data. They were wrapped in `struct` in case some other data might need to be 
transferred. `minifyijpeg.x` was written following example given in http://users.cs.cf.ac.uk/Dave.Marshall/C/node34.html,
section 'Passing Complex Data Structures'. As in the example, `union` was used to better distinguish successful and 
unsuccessful calls. 

Because this part of the project only requires single threaded server, `rpcgen -C` was used to auto-generate most of 
the necessary codes. 

Two functions needed in `minifyjpeg_main.c` were implemented in `minify_via_rpc.c`. `get_minify_client()` initializes
client call handle and it was implemented following the examples given in Oracle documentation. Timeout was set using
`clnt_control()` and was set to 5 seconds to satisfy the requirement. 

`minify_via_rpc()` makes the remote call `minifyjpeg_proc_1()` which was auto-generated and returns down-sized image.
In my implementation, timeout was detected when result was `NULL`. Ideally, timeout should be detected by comparision to 
`enum clnt_stat RPC_TIMEOUT`, but the auto-generated `minifyjpeg_proc_1()` only returns null when the call didn't succeed. 
Therefore, comparision with the `NULL` was the only way timeout could have been detected. 

The function that is used by the server to reduce the quality of an image was implemented in `minifyjpeg_proc_1_svc()`. 
After initializing `magickminify` library, a high quality image was down-sized using `magickminify()`

**Comment**

Overall, this part of the project was pretty straight forward. Most of the code was auto-generated and ones I had to write, 
I referenced http://users.cs.cf.ac.uk/Dave.Marshall/C/node34.html and Oracle's documentation (https://docs.oracle.com/cd/E19683-01/816-1435/index.html)

Only tricky part was setting up timeout. Because of how the auto-generated `minifyjpeg_proc_1()` works, it was not possible 
to use the returned `clnt_stat` to see if the call has timed out. For any kind of error, `NULL` was returned and this 
makes it harder to distinguish the type of error that has occurred. However, `union` was used for XDR output, so it is still
possible to distinguish timeout error from other types of errors, assuming server correctly implements error handling.

If I had more time, I would have liked to implement better server-side error handling that will send appropriate type of 
result to the client based on what type of error has occured. 

### Extra Credit Portion

**Overall Approach**

To support multi-threading, `rpcgen -MN` was used. Appropriate modification has been made form the codes written in 'Main Part'
to accommodate this change. 

`minifyjpeg_svc.c` was modified to make the server multi-threaded. On request, a thread is created and this thread handles 
the client request. 

**Design Details**

To simplify XDR file, `union`, which was used in 'Main Part', was no longer used. `rpcgen -MN` was used and this creates
codes that doesn't contain static data that was used in non-multithreaded 'Main Part'

Most of the codes from `minifyjpeg.c` and `minify_via_rpc.c` was reused, with appropriate modification to take into account that
 `-MN` option was used and `union` is no longer used. 
 
One significant change was that because of the change in how `minifyjpeg_proc_1()` was implemented, it was possible this time 
to use `RPC_TIMEOUT` to check if server has timed out. 
 
Most of the work was in `minifyjpeg_svc.c`. I decided to use approach which new thread was created for a new request, instead of 
having fixed number of worker thread. 

Previously, `minifyjpeg_prog_1()` used to receive request, down-size image and send client response. However, it was used as 
wrapper function to spawn a worker thread with worker function of `worker_func()`. 

It is in this `worker_func()` that all the code that used to be in `minifyjpeg_prog_1()` was moved to. 

In `worker_func()`, `svc_getargs()` retrieves global data to locally accessible memory. `struct svc_req *rqstp` is not thread-safe,
so mutex was used to make sure no other request was received by the main thread before `svc_getargs()` was finished. 
After `minifyjpeg_prog_1()` spawns a new thread on a client request, it is put into sleep until the worker function signals
that it has finished `svc_getargs()`

**Comments**

The most difficult part was making sure non-threadsafe data, like `struct svc_req *rqstp` was accessed in a controlled manner. 
Other than that, this portion of the project was mostly straight-forward due to examples like 
https://www.redhat.com/archives/redhat-list/2004-June/004164.html


## References

-https://docs.oracle.com/cd/E19683-01/816-1435/index.html, mostly Chapter 3, 4, 5, and 7. 
-http://users.cs.cf.ac.uk/Dave.Marshall/C/node34.html
-https://www.redhat.com/archives/redhat-list/2004-June/004164.html

