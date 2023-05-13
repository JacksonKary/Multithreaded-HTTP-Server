# Multithreaded-HTTP-Server
This project is a basic HTTP server that supports multithreading and uses a connection queue that's thread-safe with mutex locks. It's designed to handle only "GET" requests. It is currently set to use 5 threads, meaning it can handle 5 concurrent clients, but this macro can be increased easily.

This gives it the ability to concurrently communicate with multiple clients, something any real web server would need to do, without the high overhead of forking several client processes.

In fact, to avoid the costs of repeatedly creating and destroying threads as clients connect to and disconnect from the server, it uses a thread pool design pattern. The main thread of the server process handles accepting new client connections with accept() and then passes responsibility for communicating with the new client to a “worker” thread in the thread pool.

The key to this design is the implementation of a queue data structure to pass work from the main thread to the thread pool and to coordinate the activities of each thread in the pool. The queue stores file descriptors for the client-specific connection sockets that are generated by calls to accept().

This queue is also thread safe, meaning that even with multiple threads concurrently accessing memory storing the queue, the queue’s state and data do not become corrupted. Furthermore, the queue has some helpful timing synchronization built in. When a thread tries to add to a full queue, it is blocked until space is made available. When a thread tries to remove from an empty queue, it is blocked until a data element is made available.

This involved setting up a TCP server socket and accepting incoming client connection requests in the main() function defined in the http_server.c file and then implementing HTTP request parsing and response generation by completing the HTTP library functions defined in http.c.

## This project uses a number of important systems programming topics:

- TCP server socket setup and initialization with socket(), bind(), and listen()
- Server-side TCP communication with accept() followed by read() and write()
- HTTP request parsing and response generation
- Clean server termination through signal handling
- Thread creation and management with pthread_create() and pthread_join()
- Implementation of a thread-safe queue data structure with the pthread_mutex_t and pthread_cond_t primitives



## What is in this directory?
<ul>
  <li>  <code>http.h</code> : Header file for HTTP library functions.
  <li>  <code>http.c</code> : Implementation of HTTP library functions.
  <li>  <code>http_server.c</code> : Main HTTP Server Program.
  <li>  <code>server_files/</code> : Folder, which contains: Sample files for the server to offer to clients.
  <li>  <code>connection_queue.h</code> : Header file for thread-safe queue data structure.
  <li>  <code>connection_queue.c</code> : Implementation of thread-safe queue data structure.
  <li>  <code>testius</code> : Script for executing test cases.
  <li>  <code>test_cases/</code> : Folder, which contains: Test cases and related materials
  <li>  <code>Makefile</code> : Build file to compile and run test cases.
</ul>

## Running Tests

There are many ways to test this server. By far, the simplest method is to simply run the makefile command: <code>make test port=\<port_num></code> , where <port_num>
  is the port you want the server to bind to (default is 8000, valid range is 1024 through 65535). 

However, the most satisfying way to test this server is to actually run it  using the following command format:

<code>> ./http_server <serve_dir> \<port></code> , where <serve_dir> is the directory containing content for clients to request (To use the provided directory, use "server_files/").
  
This launches the server so it is ready to take clients. Now, connect to the server with a client. There are multiple ways to do this, but the coolest way is to send a request via an internet browser.

  Typing: <code>localhost:\<port>/\<resource></code> in your browser and pressing enter should connect, retrieve, and display the results on your screen. \<port> is the port number the server is binded to and \<resource>
  is the name of the item you're requesting from \<serve_dir> ("server_files/"). 
  
A Makefile is provided as part of this project. This file supports the following commands:

<ul>
  <li>  <code>make</code> : Compile all code, produce an executable program.
  <li>  <code>make clean</code> : Remove all compiled items. Useful if you want to recompile everything from scratch.
  <li>  <code>make clean-tests</code> : Remove all files produced during execution of the tests.
  <li>  <code>make test</code> : Run all test cases.
  <li>  <code>make test testnum=5</code> : Run test case #5 only.
  <li>  <code>make test port=4061</code> : Run all test cases with server port=4061 (valid range is 1024 through 65535).
</ul>
