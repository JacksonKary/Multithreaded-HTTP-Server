#ifndef CONNECTION_QUEUE_H
#define CONNECTION_QUEUE_H

#include <pthread.h>

#define CAPACITY 5

// Struct representing a thread-safe queue data structure
// The queue stores file descriptors of active client TCP sockets
typedef struct {
    int client_fds[CAPACITY];
    int length;
    int read_idx;
    int write_idx;
    int shutdown;
    // Necessary thread synchronization primitives to this struct
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} connection_queue_t;

/*
 * Initialize a new connection queue.
 * The queue can store at most 'CAPACITY' elements.
 * queue: Pointer to connection_queue_t to be initialized
 * Returns 0 on success or -1 on error
 */
int connection_queue_init(connection_queue_t *queue);

/*
 * Add a new file descriptor to a connection queue. If the queue is full, then
 * this function blocks until space becomes available. If the queue is shut
 * down, then no addition to the queue takes place and an error is returned.
 * queue: A pointer to the connection_queue_t to add to
 * connection_fd: The socket file descriptor to add to the queue
 * Returns 0 on success or -1 on error
 */
int connection_enqueue(connection_queue_t *queue, int connection_fd);

/*
 * Remove a file descriptor from the connection queue. If the queue is empty,
 * then this function blocks until an item becomes available. If the queue is
 * shut down, then no removal from the queue takes place and an error is
 * returned.
 * queue: A pointer to the connection_queue_t to remove from
 * Returns the removed socket file descriptor on success or -1 on error
 */
int connection_dequeue(connection_queue_t *queue);

/*
 * Cleanly shuts down the connection queue. All threads currently blocked on an
 * enqueue or dequeue operation are unblocked and an error is returned to them.
 * queue: A pointer to the connection_queue_t to shut down
 * Returns 0 on success or -1 on error
 */
int connection_queue_shutdown(connection_queue_t *queue);

/*
 * Deallocates and cleans up any resources associated with a connection queue.
 * Returns 0 on success or -1 on error
 */
int connection_queue_free(connection_queue_t *queue);

#endif // CONNECTION_QUEUE_H
