#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include "queue.h"

/**
 * This structure should be dynamically allocated and passed as
 * an argument to your thread using pthread_create.
 * It should be returned by your thread so it can be freed by
 * the joiner thread.
 */
struct thread_data{
    // thread data
    pthread_mutex_t *mutex;
    pthread_t *thread;
    bool thread_complete_success;

    // data for connections
    int client_fd;
    struct sockaddr_storage client_addr;
    socklen_t addr_len;

    TAILQ_ENTRY(thread_data) entries;
};
