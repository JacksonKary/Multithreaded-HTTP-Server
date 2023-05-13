#define _GNU_SOURCE

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

#define SERVER_FILE_PREFIX "server_files/"
#define CONCURRENCY_DEGREE 5

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int n_waiters = 0;
static int semaphore_initialized = 0;
static sem_t semaphore;

/*
 * Versions of (f)open that will only allow threads to proceed once a sufficient
 * number of threads have initiated an (f)open syscall
 * Specifically, it blocks all calling threads until 'CONCURRENCY_DEGREE'
 * threads have made a call to (f)open().
 * This is a (probably inelegant) way to check if a program is really capable
 * of 'CONCURRENCY_DEGREE' threads of execution.
 */

// Initializes the semaphore if not initialized already
// Returns 0 on success, -1 on failure
int init_semaphore(void) {
    int result;
    if ((result = pthread_mutex_lock(&lock)) != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }

    if (semaphore_initialized == 0) {
        if (sem_init(&semaphore, 0, 0) == -1) {
            perror("sem_init");
            pthread_mutex_unlock(&lock);
            return -1;
        }
        semaphore_initialized = 1;
    }

    if ((result = pthread_mutex_unlock(&lock)) != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

// Returns true if the pathname provided is to a server file, false otherwise
int is_server_file(const char *pathname) {
    return (strncmp(SERVER_FILE_PREFIX, pathname, strlen(SERVER_FILE_PREFIX)) == 0);
}

// Wait until 'CONCURRENCY_DEGREE' threads all have initiated barrier(). Then,
// allow all of them to proceed.
int barrier(void) {
    int result;
    if ((result = pthread_mutex_lock(&lock)) != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    if (n_waiters == CONCURRENCY_DEGREE - 1) {
        for (int i = 0; i < CONCURRENCY_DEGREE - 1; i++) {
            if (sem_post(&semaphore) == -1) {
                perror("sem_post");
                pthread_mutex_unlock(&lock);
                return -1;
            }
        }
        n_waiters = 0;
        if ((result = pthread_mutex_unlock(&lock)) != 0) {
            fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
            return -1;
        }
    } else {
        n_waiters++;
        if ((result = pthread_mutex_unlock(&lock)) != 0) {
            fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
            return -1;
        }
        if (sem_wait(&semaphore) == -1) {
            perror("sem_wait");
            return -1;
        }
    }

    return 0;
}

int open(const char *pathname, int flags, ...) {
    // Init the semaphore if it hasn't already been initialized
    if (init_semaphore() != 0) {
        return -1;
    }

    int (*open_orig)(const char *pathname, int flags);
    open_orig = dlsym(RTLD_NEXT, "open"); // Get pointer to real open
    char *error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym: %s\n", error);
        return -1;
    }

    // If thread isn't opening a server file, let it proceed
    if (!is_server_file(pathname)) {
        return open_orig(pathname, flags);
    }

    // Otherwise, check in at the barrier
    int barrier_checkin = barrier();
    if (barrier_checkin != 0) {
        return -1;
    }

    return open_orig(pathname, flags);
}

FILE *fopen(const char * restrict path, const char * restrict mode) {
    // Init the semaphore if it hasn't already been initialized
    if (init_semaphore() != 0) {
        return NULL;
    }

    FILE* (*fopen_orig)(const char * restrict path, const char * restrict mode);
    fopen_orig = dlsym(RTLD_NEXT, "fopen"); // Get pointer to real fopen
    char *error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym: %s\n", error);
        return NULL;
    }

    // If thread isn't opening a server file, let it proceed
    if (!is_server_file(path)) {
        return fopen_orig(path, mode);
    }

    // Otherwise, check in at the barrier
    int barrier_checkin = barrier();
    if (barrier_checkin != 0) {
        return NULL;
    }

    return fopen_orig(path, mode);
}
