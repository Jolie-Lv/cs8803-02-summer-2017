# Project README file

Author: Jongho Jung (jjung327)

## Project Description

### Warm-up
#### Echo Client-Server

Design of this warm-up is very simple. The server initializes and listens for a connection from client. When there is a data sent from
the client, the server reads the content and writes it back to the socket. 

The client initializes and write a content to the socket. After the writing, the client listens for response from the server.

#### Transferring a File

I used basic socket setup and connect code I wrote for echoclient.c and echoserver.c, and added parts
for file read and write. 

For transferserver.c, when a connection is accepted from a client, it opens a file, reads the file, and sends it over the socket.

Initially, I was thinking about creating a buffer for reading file content, read file onto the buffer using read(), and send the buffer using write() onto the client socket. These operations would be performed in loops to read and write file data in chunks.

However, after some research, I discovered sendfile() which encapsulates series of operations I mentioned above. sendfile() is what I used on server to read file and send it to clients.

For transferclient.c, when a connection is requested, a file is opened and data received from server is written onto that file until there is no more data read from the socket. 

Biggest problem was making sure appropriate access permission was given. Initially, I thought file descriptor in transferclient.c would only need write access, but I had to also give read access to make the code work.

### Implementing the Getfile Protocol
#### gfserver.c

For functions that are big, I tried dividing into smaller functions. For example, instead of having all the code in `gfserver_serve`, I refactored
some of the functionality into smaller functions like `start_sever`, `parse_request`, and `send_unsuccessful_response`. 

The biggest challenge was making sure all the error cases are covered and parsing/formatting request/response string. 

If I were to redo this part, I would try to do better job on refactoring since some functions are slightly too big.

#### gfclient.c

As in `gfserver.c`, I tried to divide big functions into multiple smaller functions. 

There weren't much issues other than not reading full amount of data occasionally. Often, read from socket to receive server response 
stalls at the very last chunk. I have tried many methods of solving this problem by varying buffer size or initializing the buffer, but 
wasn't able to find any solution.

If I were to do this project again, this would be the issue I would focus the most. 
	
### Implementing a Multithreaded Getfile Server
#### gfclient_download.c

**Overall Approach**

Initially, the gfclient_download.c make requests in a single thread inside `main()`. 
Main modification that was made involved moving codes for making requests to server into a separate function so that requests 
can be make inside a thread. 

**Design**

`main()` now only deals with (1) parsing arguments, (2) initialize worker thread pool, (3) queuing up requests into `request_queue`.
Sequence of tasks `main()` does can be summarized as following: (1) parse arguments, (2) initialize variables 
(e.g. gfc, request queue, mutex, condition variables), (3) initialize worker threads with function `assign_worker_to_task`, 
(4) queuing requests into request queue, `request_queue`, (4) set variable `queuing_request_finished` which is used to indicate
`main()` thread has finished queuing requests, (5) wait for worker threads to finish all the request tasks and join, and (6)
finally free up memory.

`assign_worker_to_task` is the function each worker threads executes. It runs indefinitely inside for-loop until `request_queue`
is empty AND `main()` thread has finished queuing up the requests. Worker threads wait until there is an item in `request_queue`.
When there is an item, which is broad casted by `main()`, it performs making request to server. Once all the items in queue
are fulfilled (i.e. no item left in queue) and `main()` updated `queuing_request_finished` variable to indicate it has finished
adding request to the queue, worker threads exit and join the main thread.

**Comments**

The hardest problems I faced were ones relating to memory leaks and how to notifying worker threads when to exit. 

I tried to account for 
cases where queuing up in main thread might not be as fast as fulfilling the requests in the queue by worker threads. This means
having an empty request queue shouldn't necessarily indicate worker threads to exit because main thread might be adding more 
requests in the queue. Therefore, I added another variable, `queuing_request_finished`, and worker threads can only exit when
this flag is set as well as queue being empty.

If I were to work on this project again, this would be something I would try to improve since the method that I currently am using
feels like it isn't the most elaborate method.

I tried testing this code by changing number of threads and requests per thread. Few cases I used were when both numbers are
small, both numbers are big, and when number of threads are larger than number of requests per thread

#### gfserver_main.c

**Overall Approach**

Initially, the gfserver_main.c handles request in a single thread whenever registered handler function was called. 
Main modification that was made involved adding a wrapper request handler which would queue request and notify workers to 
fulfill the queued requests. 

**Design**

Sequence of tasks `main()` does can be summarized as following: (1) parse arguments, (2) initialize variables 
(e.g. gfc, request queue, mutex, condition variables), (3) register wrapper handler, `client_request_handler`, instead of the
actual handler, `handler_get`, (4) initialize worker threads, (5) run `gfserver_serve` to listen to client requests.

Now that `client_request_handler` is registered to `gfs`, this function would be called every time client makes a request.
All `client_request_handler` does is queue the request path (along with other data) and broadcast worker threads.

`assign_worker_to_task` is the function each worker threads executes. It runs indefinitely inside for-loop. Worker threads wait 
until there is an item in `client_request_queue`. When there is an item, which is broad casted by `client_request_handler`, 
it calls `handler_get` with request path (and other data) dequeued.

**Comments**

Contrary to project readme file, I didn't need to modify `handler.c`. 

I tried testing this code by changing number of threads and number of requests client make.

## Known Bugs/Issues/Limitations

It was a bit too hard to understand the response from bonnie submission. Especially, there were many instances where it worked 
fine on local machine, but didn't work on bonnie and it could be very difficult to debug. 

## References

- https://linux.die.net/man/ 
- https://www.tutorialspoint.com/cprogramming
- https://stackoverflow.com

