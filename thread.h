#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ucontext.h>

//macros
#define NUM_THREADS 15
#define MEM 16384
#define LEVELS 16
#define QUANT 50000
#define STARVATION_THRESH 50

//thread states
typedef enum state {
    READY, RUNNING, WAITING, YIELDED, DONE
} state;

//thread struct
typedef struct thread_t {
    ucontext_t uc;
    struct thread_t *next;
    state thread_state;
    long int id;
    int running_time;
    int priority;
    void *return_val;
    long int last_exec_time;
} thread_t;

//queue struct
typedef struct {
    thread_t *front;
    thread_t *back;
    int ctr;
} queue;

//mutex struct
typedef struct {
    int lock;
    thread_t *owner;
    queue *wait;
} thread_mutex_t;

//queue stuff
void queue_init(queue *q);
void enqueue(queue *q, thread_t *thread);
thread_t *dequeue(queue *q);

//threading stuff
int thread_create(thread_t *thread, pthread_attr_t *attr, void *(*function)(void *), void *arg);
void thread_yield();
void thread_exit(void *value_ptr);
int thread_join(thread_t *thread, void **value_ptr);

//mutex stuff
int thread_mutex_init(thread_mutex_t *mutex, const pthread_mutexattr_t *mutextattr);
int thread_mutex_lock(thread_mutex_t *mutex);
int thread_mutex_unlock(thread_mutex_t *mutex);
int thread_mutex_destroy(thread_mutex_t *mutex);

//wrapper to execute functions in threads
void thread_runner(thread_t *thread_node, void *(*f)(void *), void *arg);

//scheduler stuff
void scheduler_init();
void scheduler();
void scheduler_add_thread(thread_t *thread_node, int priority);
thread_t *scheduler_get_next_thread();

long int time_stamp();