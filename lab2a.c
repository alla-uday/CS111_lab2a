#define _GNU_SOURCE
#include <math.h>
#include <stdint.h>/* for uint64 definition */
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>/* for clock_gettime */

#define BILLION 1000000000L

/* struct that holds the pointer to the counter and values to be incremented by */
struct arg_struct {
  long long *counter;
  long long value;
  long long value1;
};

/* holds number of iterations */
int iterations;

/* holds 1 if yield option is enabled */
int opt_yield;

/* mutex to implement lock */
pthread_mutex_t count_mutex;

/* lock for implementing test_and_set */
volatile int lock=0;

/* variables to hold which sync option has been passed */
int m=0;
int s=0;
int c=0;

/* add function */
void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (opt_yield)
    pthread_yield();
  *pointer = sum;
}

/* add function for Compare and Swap */
void addCAS(long long *pointer, long long value)
{
  long long old,sum; //long long sum=*pointer+value;

   do{
     old = *pointer;
     sum= *pointer + value;
     if (opt_yield)
       pthread_yield();
   }while (!__sync_bool_compare_and_swap(pointer, old, old +value));//{
  
}

/* thread function */
void *thr_func(void *arg) {
  struct arg_struct *args = (struct arg_struct *)arg;
  int i;
  /* if mutex protection is enabled */
  if(m==1){
    /* incrementing by 1*/
    for(i=0;i<iterations;i++){
      pthread_mutex_lock(&count_mutex);
      add(args->counter,args->value);
      pthread_mutex_unlock(&count_mutex);
    }
    /* decrementing by 1*/
    for(i=0;i<iterations;i++){
      pthread_mutex_lock(&count_mutex);
      add(args->counter,args->value1);
      pthread_mutex_unlock(&count_mutex);
    }
  }
  /* if spin lock is enabled */
  else if(s==1){
    for(i=0;i<iterations;i++){
      /* incrementing counter by 1*/
      while (__sync_lock_test_and_set(&lock, 1));
      add(args->counter,args->value);
      __sync_lock_release(&lock);
    }
    for(i=0;i<iterations;i++){
      /* decrementing counter by 1*/
      while (__sync_lock_test_and_set(&lock, 1));
      add(args->counter,args->value1);
      __sync_lock_release(&lock);
    }
  }
  /* CAS implementation */
  else if(c==1){
    for(i=0;i<iterations;i++){
      /* incrementing 1 */
      addCAS(args->counter,args->value);
    }
    for(i=0;i<iterations;i++){
      /* decrementing 1 from counter */
      addCAS(args->counter,args->value1);
    }
  }
  /* No protection enabled*/
  else{
    for(i=0;i<iterations;i++){
      add(args->counter,args->value);
    }
    for(i=0;i<iterations;i++){
      add(args->counter,args->value1);
    }
  }
  pthread_exit(NULL);
}

int main(int argc, char **argv)
{
  int numthreads=1;
  iterations=1;
  int opt=0;
  int longIndex=0;
  char* syncopt="";
  static const char *optString = "tisy";
  static struct option long_options[] = {
    {"threads", required_argument, NULL, 't'},
    {"iterations", required_argument, NULL,'i'},
    {"sync", required_argument, NULL,'s'},
    {"yield", no_argument, NULL,'y'},
    {NULL, no_argument, NULL, 0}
  };
  opt = getopt_long( argc, argv, optString, long_options, &longIndex );
  while( opt!= -1) {
    switch( opt ) {
    case 't':
      numthreads=atoi(optarg);
      break;
    case 'i':
      iterations=atoi(optarg);
      break;
    case 's':
      syncopt=optarg;
      break;
    case 'y':
      opt_yield=1;
      break;
    default:
      /* You won't actually get here. */
      exit(4);
      break;
    }
    opt = getopt_long( argc, argv, optString, long_options, &longIndex );
  }
  if(strcmp(syncopt,"")!=0){
    if(strcmp(syncopt,"m")==0){
      m=1;
      if (pthread_mutex_init(&count_mutex, NULL) != 0)
	{
	  printf("\n mutex init failed\n");
	  return 1;
	}
    }
    else if(strcmp(syncopt,"c")==0){
      c=1;
    }
    else if(strcmp(syncopt,"s")==0){
      s=1;
    }
  }
  uint64_t diff;
  struct timespec start, end;
  // pthread_t thr[numthreads];
   pthread_t *thr=malloc(numthreads*sizeof(pthread_t));
  pthread_attr_t attr;
  void *status;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
  /* counter */
  long long counter; 
  counter=0;
  struct arg_struct args1;
  args1.counter=&counter;
  args1.value=1;
  args1.value1=-1;
  int i, rc;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < numthreads; ++i) {
    if ((rc = pthread_create(&thr[i], &attr, &thr_func, (void *)&args1))) {
    //if ((rc = pthread_create(&thr[i], NULL, &thr_func, (void *)&args1))) {
      fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
      exit(EXIT_FAILURE);
    }
  }
  for (i = 0; i < numthreads; ++i) {
    pthread_join(thr[i], NULL);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);/* mark the end time */
  diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  int operations= 2*iterations*numthreads;
  long long unsigned int avgop=diff/operations;
  if(counter!=0){
    fprintf(stderr,"%d threads x %d iterations x (add + subtract) = %d operations \n",numthreads,iterations,operations);
    fprintf(stderr,"ERROR: final count = %lld\n",counter);  
    fprintf(stderr,"elapsed time: %lluns\n",(long long unsigned int) diff);
    fprintf(stderr,"per operation: %lluns\n",(long long unsigned int) avgop);
    exit(1);
  }
  printf("%d threads x %d iterations x (add + subtract) = %d operations \n",numthreads,iterations,operations);
  //printf("final count = %lld\n",counter);
  printf("elapsed time: %lluns\n",(long long unsigned int) diff);
  printf("per operation: %lluns\n",(long long unsigned int) avgop);
  exit(0);
  
}
