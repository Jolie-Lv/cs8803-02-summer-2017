
# Project README file

Author: Jongho Jung (jjung327)

## Project Description

### Part 1

**Overall Approach**

This part of the project is fairly straight forward where I had to implement `handle_with_curl()` 
and appropriately call it from `webproxy.c`. 

**Design Details**

`webproxy.c` does global initialization and cleanup of `curl`, and it also registered call-back on request from client. 

Instead of directly registering `handle_with_curl()` to `GFS_WORKER_FUNC`, a wrapper function `handle_request()` is used
to convert parameter `char* path` to url by pre-pending server address and pass this url to `handle_with_curl()`.

`handle_with_curl()` initializes `easy_curl` session, registeres appropriate options such as url, write callback, etc..., 
and performs cleanup & error handling after `easy_curl` finishes performing. 

From `write_callback()` registered to `CURLOPT_WRITEFUNCTION`, length of the content is determined, `gfs_sendheader` is called,
and `gfs_send` is called everytime call back is called for any data received. 

**Comments**

The biggest challenge for this part was to determine length of the content and call `gfs_sendheader` only once 
before calling `gfs_send`. I tried to use existing functions from the library and didn't want to parse header myself. 
Using `curl_easy_getinfo` I was able to find content length from the header without building my own parser. 

I also had to make sure that `gfs_sendheader` with this information is only called once and before `gfs_send`. Therefore, 
`gfs_sendheader` also needs to be called inside `write_callback()` right before `gfs_send()`. Another option was calling it
inside callback for header received, but this callback is called multiple times when each line of header was received, so I decided
to put `gfs_sendheader` inside `write_callback()` which is called less frequently. 

I also used flag to make sure `gfs_sendheader` is only called once. 
	
## Part 2

**Overall Approach**

For this project, I used SysV Shared Memory API to implement transfer of file between `webproxy` and `simplecached` processes.

In big picture, when `webproxy` receives client request, it assigns a shared memory segment to the worker thread that processes
the request. The worker thread use that segment and message queue to communicate to `simplecached` process to retrieve cached
request, if it exists in cache. 

**Design Details**

`webproxy.c` registers `handle_with_cache()` via a wrapper function `client_request_handler()`. In `webproxy,c`, ids of free
segments are stored in a queue. Note that this is just a unique id used to identify the segments. They are not initialized this code, 
but initialized on demand, if it hasn't already been done, from `simplecached` process. However, cleanup of the shared memory is 
performed in `webproxy.c`. 

The wrapper function `client_request_handler()` checks to see if there is any free segment in the queue and if there is, 
assigns that segment to the worker process. Mutex is used to ensure concurrency among multiple worker threads. 

Once the worker thread has a shared memory segment to use, `handle_with_cache()` is used to send a message through IPC message queue
to `simplecached` process to request for the asset client has requested. 

`simplecached.c` runs in infinite loop to see if there is any message in request message queue. Once it receives a request, 
it checks to see if a cache exist for the request using `simplecache.h` library. 

If there is a cache, `simplecached.c` calls `from_cached_to_shared_memory()` from `shm_channel.h` library to write the cached
file into shared memory where `webproxy` process can access. Once there is a write to the shared memory, `from_cached_to_shared_memory()`
sends a response message to `webproxy` to let it know that there is data it needs to retrieve.

`handle_with_cache()`, using data from receiving response from `simplecached` process, sends header to client via `gfs_sendheader()`. 
Then, `handle_with_cache()` reads the data in shared memory and calls `gfs_send` to send the data to client. 
 
 If more data needs to be received, which can be determined from response message, `handle_with_cache()` send message to `simplecached`
 to indicate that it has finished reading the data from shared memory and new data can be written there. 
 
 `simplecached`, upon receiving the message, writes more data to the shared memory and sends a response to `webproxy` again to let
 it know that there is a new data it could read from. This process repeats until full data is transferred.
 
 **Comments**
 
 Biggest challenge in this portion of the project was making sure everything is done in right order. `webproxy` and `simplecached`
 needs to communicate with each other constantly to relay requests and responses. 
 
 I used waiting/sending IPC message to make sure concurrency, instead of other methods like semaphore. This was possible because
 each memory segment is used only by a single worker in `webproxy` and `simplecached` only use the segment indicated in the message.
 A worker in `webproxy` would send message to `simplecached` to tell that it is ready to read data from the segment in shared
 memory and `simplecached` would only write data once it receives that message. Also, `webproxy` would only read data from the segment
 when it receives the message from `simplecached`. This ensures data is only read and written at the right time.


## References

- http://beej.us/guide/bgipc/output/html/singlepage/bgipc.html#intro, chapter 7 and 9.
- https://curl.haxx.se/libcurl/c/


