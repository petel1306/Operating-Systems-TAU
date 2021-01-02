#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_THREADS 15
int counter = 0;
pthread_mutex_t lock;

//====================================================
void *next_counter(void *t) {
  long tid = (long)t;
  double result = 0.0;
  int rc;

  printf("Thread %ld starting...\n", tid);

  for (int i = 0; i < 10000000; ++i) {
    rc = pthread_mutex_lock(&lock);
    if (0 != rc) {
      printf("ERROR in pthread_mutex_lock(): "
             "%s\n",
             strerror(rc));
      exit(-1);
    }

    ++counter;

    rc = pthread_mutex_unlock(&lock);
    if (0 != rc) {
      printf("ERROR in pthread_mutex_unlock(): "
             "%s\n",
             strerror(rc));
      exit(-1);
    }
  }
  printf("Thread %ld done. Result = %d\n", tid, counter);
  pthread_exit((void *)t);
}

//====================================================
int main(int argc, char *argv[]) {
  pthread_t thread[NUM_THREADS];
  int rc;
  void *status;

  // --- Initialize mutex ---------------------------
  rc = pthread_mutex_init(&lock, NULL);
  if (rc) {
    printf("ERROR in pthread_mutex_init(): "
           "%s\n",
           strerror(rc));
    exit(-1);
  }

  // --- Launch threads ------------------------------
  for (long t = 0; t < NUM_THREADS; ++t) {
    printf("Main: creating thread %ld\n", t);
    rc = pthread_create(&thread[t], NULL, next_counter, (void *)t);
    if (rc) {
      printf("ERROR in pthread_create():"
             " %s\n",
             strerror(rc));
      exit(-1);
    }
  }

  // --- Wait for threads to finish ------------------
  for (long t = 0; t < NUM_THREADS; ++t) {
    rc = pthread_join(thread[t], &status);
    if (rc) {
      printf("ERROR in pthread_join(): "
             "%s\n",
             strerror(rc));
      exit(-1);
    }
    printf("Main: completed join with thread %ld "
           "having a status of %ld\n",
           t, (long)status);
  }

  // ---  Epilogue -----------------------------------
  printf("Main: program completed. Exiting."
         "  Counter = %d\n",
         counter);

  pthread_mutex_destroy(&lock);
  pthread_exit(NULL);
}
//=================== END OF FILE ====================
