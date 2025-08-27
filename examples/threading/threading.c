#include "threading.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Optional: use these functions to add debug or error prints to your
// application
#define DEBUG_LOG(msg, ...)
// #define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

int msleep(unsigned int tms) { return usleep(tms * 1000); }

void *threadfunc(void *thread_param) {

  // TODO: wait, obtain mutex, wait, release mutex as described by thread_data
  // structure hint: use a cast like the one below to obtain thread arguments
  // from your parameter
  // struct thread_data* thread_func_args = (struct thread_data *) thread_param;

  struct thread_data *thread_func_args = (struct thread_data *)thread_param;

  pthread_mutex_lock(&lock);

  msleep(12);

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

  // create thread
  // sleep for ms
  // obtain mutex
  // hold for ms
  // release

  pthread_create(pthread_t, NULL, foo, NULL);

  return false;
}

int main() {

  pthread_t thread;

  // create thread
  pthread_create(&thread, NULL, foo, NULL);

  // wait for thread
  pthread_join(thread, NULL);
}