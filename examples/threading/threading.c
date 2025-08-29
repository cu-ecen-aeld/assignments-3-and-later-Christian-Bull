#include "threading.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h> 

// Optional: use these functions to add debug or error prints to your
// application
#define DEBUG_LOG(msg, ...)
// #define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

void *threadfunc(void *thread_param) {

  // TODO: wait, obtain mutex, wait, release mutex as described by thread_data
  // structure hint: use a cast like the one below to obtain thread arguments
  // from your parameter
  // struct thread_data* thread_func_args = (struct thread_data *) thread_param;

  struct thread_data* thread_func_args = (struct thread_data *) thread_param;

  printf("starting threadfunc\n");

  // wait to lock mutex
  usleep(thread_func_args->wait_to_obtain_ms*1000);

  // obtain mutex
  printf("locking mutex\n");
  int rcm = pthread_mutex_lock(thread_func_args->mutex);
  if (rcm != 0) {
    perror("error locking mutex\n");
    thread_func_args->thread_complete_success = false;
    pthread_exit(thread_func_args);
  }

  printf("mutex locked\n");

  // wait before releasing
  usleep(thread_func_args->wait_to_release_ms*1000);

  // unlock
  printf("unlocking mutex\n");
  int rcu = pthread_mutex_unlock(thread_func_args->mutex);
  if (rcu !=0) {
    perror("error unlocking mutex\n");
    thread_func_args->thread_complete_success = false;
    pthread_exit(thread_func_args);
  }

  thread_func_args->thread_complete_success = true;
  pthread_exit(thread_func_args);

  return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,
                                  int wait_to_obtain_ms,
                                  int wait_to_release_ms) {
  /**
   * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass
   * thread_data to created thread using threadfunc() as entry point.
   *
   * return true if successful.
   *
   * See implementation details in threading.h file comment block
   */

  struct thread_data *thread_d = malloc(sizeof(struct thread_data));
  
  thread_d->wait_to_obtain_ms = wait_to_obtain_ms;
  thread_d->wait_to_release_ms = wait_to_release_ms;
  thread_d->mutex = mutex;
  thread_d->thread_complete_success = false;

  int rc = pthread_create(thread, NULL, threadfunc, thread_d);
  if (rc != 0) {
    perror("pthread_create");
    return false;
  }

  int rcx = pthread_join(*thread, NULL);
  if (rcx != 0) {
    printf("pthread_join failed with error %d\n", rcx);
    return false;
  }

  printf("thread completed\n");
  free(thread_d);

  return true;
}

// int main() {

//   pthread_t *thread;
//   pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//   int wait = 1000;
//   int release = 10;

//   printf("starting test thread\n");

//   bool test = (start_thread_obtaining_mutex(thread, &mutex, wait, release));

//   usleep(1000);

//   printf("Results: %d", test);
// }