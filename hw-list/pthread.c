/*
 * You may NOT modify this file. Any changes you make to this file will not
 * be used when grading your submission.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>

#define NUM_THREADS 4
int common = 162;
char* somethingshared;

void* threadfun(void* threadid) {
  long sum = 0;
  long tid = (long)threadid;
    int cpu = sched_getcpu();  // returns the core ID
    printf("Thread %ld running on CPU core %d\n", tid, cpu);
    for (long i = 0; i < 1000000000L; i++) sum += i;
    printf("Thread %ld done: %ld\n", (long)threadid, sum);
    pthread_exit(NULL);
  // long tid;
  // tid = (long)threadid;
  // printf("Thread #%lx stack: %lx common: %lx (%d) tptr: %lx\n", tid, (unsigned long)&tid,
  //        (unsigned long)&common, common++, (unsigned long)threadid);
  // printf("within threadfun for tid %lx: %lx: %s\n", tid, (unsigned long)somethingshared, somethingshared + tid);
  // pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
  int rc;
  long t;
  int nthreads = NUM_THREADS;
  char* targs = strcpy(malloc(100), "I am on the heap.");

  if (argc > 1) {
    nthreads = atoi(argv[1]);
  }

  pthread_t threads[nthreads];

  printf("Main stack: %lx, common: %lx (%d)\n", (unsigned long)&t, (unsigned long)&common, common);
  // printf("targs: %i\n", targs[18]);
  // printf("heap ptr: %p\n", targs);
  puts(targs);
  somethingshared = targs;
  for (t = 0; t < nthreads; t++) {
    printf("main: creating thread %ld\n", t);
    rc = pthread_create(&threads[t], NULL, threadfun, (void*)t);
    if (rc) {
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
  }

  /* Last thing that main() should do */
  pthread_exit(NULL);
}
