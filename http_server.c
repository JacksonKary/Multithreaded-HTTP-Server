#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "connection_queue.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

const char *serve_dir;
int keep_going = 1;

void handle_sigint(int signo) {
    keep_going = 0;
}

void* worker_func(void* arg) {
    int client_fd;
    while (1) {
        client_fd = connection_dequeue(arg);
        if (client_fd == -1){
            return NULL;
        }
        // Read client's request
        char resource_name[BUFSIZE];
        memset(resource_name, 0, BUFSIZE);
        int read_status = read_http_request(client_fd, resource_name);
        if (read_status != 0) {
            perror("read_http_request");
            close(client_fd);
        }
        // Construct resource_path
        char resource_path[BUFSIZE];
        memset(resource_path, 0, BUFSIZE);
        sprintf(resource_path, "%s%s", serve_dir, resource_name);  // Assume resource_name follows format "/resource"
        // Write response to client
        int write_status = write_http_response(client_fd, resource_path);
        if (write_status != 0) {
            perror("write_http_response");
            close(client_fd);
        }
        if (close(client_fd) == -1) {
            perror("close");
        }
    }
}

int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    // Store command line arguments
    serve_dir = argv[1];
    const char *port = argv[2];

    // Catch SIGINT so we can clean up properly
    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    if (sigfillset(&sigact.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sigact.sa_flags = 0; // Note the lack of SA_RESTART
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
    // Set up hints - we'll take either IPv4 or IPv6, TCP socket type
    struct addrinfo hints;
    if (memset(&hints, 0, sizeof(hints)) == NULL) {
        printf("memset");
        return 1;
    }
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // We'll be acting as a server
    struct addrinfo *server;
    // Set up address info for socket() and connect()
    int ret_val = getaddrinfo(NULL, port, &hints, &server);
    if (ret_val != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret_val));
        return 1;
    }
    // Initialize socket file descriptor
    int sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        return 1;
    }

    // Bind socket to receive at a specific port
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(server);
        close(sock_fd);
        return 1;
    }
    // No longer needed
    freeaddrinfo(server);

    // Designate socket as a server socket
    if (listen(sock_fd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(sock_fd);
        return 1;
    }
    // Queue structure for holding clients
    connection_queue_t queue;
    if (connection_queue_init(&queue) == -1) {
        printf("connection_queue_init");
        close(sock_fd);
        return 1;
    }
    // Block all signals during creation of threads
    sigset_t mask;
    if (sigfillset(&mask) == -1) {
        perror("sigfillset");
        close(sock_fd);
        return 1;
    }
    // Save original mask
    sigset_t old_mask;
    if (sigprocmask(SIG_SETMASK, &mask, &old_mask) == -1) {
        perror("sigprocmask");
        close(sock_fd);
        return 1;
    }
    // Create array of threads
    pthread_t worker_threads[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) {
        pthread_create(&worker_threads[i], NULL, worker_func, &queue);
    }
    // Restore original mask
    if (sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
        perror("sigprocmask");
        keep_going = 0;
    }

    // Main client processing/accepting loop
    while (keep_going != 0) {
        // Wait to receive a connection request from client
        // Don't bother saving client address information
        int client_fd = accept(sock_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno != EINTR) {
                perror("accept");
                close(sock_fd);
                return 1;
            } else {
                // SIGINT received: exit loop
                break;
            }
        }
        // Add new client to the connection queue
        if (connection_enqueue(&queue, client_fd) == -1) {
            perror("connection_enqueue");
            keep_going = 0;
        }
    }
    // If program receives SIGINT, shut down the client queue and wait for threads
    int return_val = 0;
    if (keep_going == 0) {
        if (connection_queue_shutdown(&queue) == -1) {
            perror("connection_queue_shutdown");
            return_val = 1;
        }
        for (int i = 0; i < N_THREADS; i++) {
            int join_val = pthread_join(worker_threads[i], NULL);
            if (join_val != 0) {
                fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(join_val));
                return_val = 1;
            }
        }
    }
    // Cleanup - reached even if we had SIGINT
    if (close(sock_fd) == -1) {
        perror("close");
        return 1;
    }
    return return_val;
}