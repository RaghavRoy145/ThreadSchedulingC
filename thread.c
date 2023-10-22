#include "thread.h"

static long int thread_counter = 0;
static long int starvation_counter = 0;
static thread_t *threads;
static thread_mutex_t *m;
static int var_w_mutex = 50;
static int var_wo_mutex = 50;
static int first_thread = 0;

//scheduler_init()
static queue *mlfq, *wait;
static thread_t *current_thread, *main_thread;
static long int number_of_threads;

//store ret val on exit or join
static void *value_ptr;

//record start_time, end_time, start_of_execution, end_of_execution
long int time_stamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return 1000000 * tv.tv_sec + tv.tv_usec; //convert to microseconds
}

//queue funcs

void queue_init(queue *q) {
    q->front = NULL;
    q->back = NULL;
    q->ctr = 0;
}

void enqueue(queue *q, thread_t *thread) {
    if(q->back != NULL) {
        q->back->next = thread;
        q->back = thread;
    }
    else {
        q->front = thread;
        q->back = thread;
    }
    q->ctr++;
}

thread_t* dequeue(queue *q) {
    thread_t *temp;
    if(q->front == NULL) {
        return NULL;
    }
    else if(q->ctr == 1) {
        temp = q->front;
        q->front = NULL;
        q->back = NULL;
    }
    else if(q->ctr > 1) {
        temp = q->front;
        q->front = q->front->next;
    }
    temp->next = NULL;
    q->ctr--;
    return temp;
}

//scheduler funcs

void scheduler() {
    struct itimerval timer;
    ucontext_t uc_temp; //store context of main thread

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL); //timers run in real time

    thread_t *temp = current_thread;

    if(temp != NULL) {
        int current_priority = temp->priority;
        temp->running_time += QUANT;

        if(temp->thread_state == WAITING) {
            current_thread = scheduler_get_next_thread();

            if(current_thread != NULL) {
                current_thread->thread_state = RUNNING;
            }
        }
        else if(temp->thread_state == DONE) {
            current_thread = scheduler_get_next_thread();
            if (current_thread != NULL) {
                current_thread->thread_state = RUNNING;
            }
        }
        else if(temp->thread_state == YIELDED) {
            //put thread back in the original queue
            scheduler_add_thread(temp, temp->priority);
            current_thread = scheduler_get_next_thread();
            if(current_thread != NULL) {
                current_thread->thread_state = RUNNING;
            }
        }
        //thread runs through its time slice
        else if(temp->running_time >= (current_priority+1)*QUANT) {
            int new_priority;
            
        }
    }
}