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
            if(current_priority + 1 < LEVELS) {
                new_priority = current_priority + 1;
            }
            else {
                scheduler_add_thread(temp, new_priority);
            }
            current_thread = scheduler_get_next_thread();
            if(current_thread != NULL) {
                current_thread->thread_state = RUNNING;
            }
        }
    }
    //thread is NULL
    else {
        current_thread = scheduler_get_next_thread();
        if(current_thread != NULL) {
                current_thread->thread_state = RUNNING;
            }
    }
    //update multi-level feedback queue based on starvation
    starvation_counter++;
    if(starvation_counter > STARVATION_THRESH - 1) {
        starvation_counter = 0;
        int i;
        for(i = 0; i < LEVELS; i++) {
            if((mlfq+i)->front != NULL) {
                thread_t *curr_thread, *prev_thread;
                curr_thread = (mlfq+i)->front;
                prev_thread = NULL;
                while(curr_thread != NULL) {
                    if((time_stamp() - curr_thread->last_exec_time) > (QUANT * STARVATION_THRESH)) {
                        if(prev_thread == NULL) {
                            (mlfq+i)->front = curr_thread->next;
                        }
                        else {
                            prev_thread = curr_thread->next;
                        }
                        //add this thread to highest priority queue
                        scheduler_add_thread(curr_thread, 0);
                    }
                    else {
                        prev_thread = curr_thread;
                    }
                    curr_thread = curr_thread->next;
                }
            }
        }
    }
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = QUANT;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    //set timer
    setitimer(ITIMER_REAL, &timer, NULL);

    //context switch
    if(current_thread != NULL) {
        current_thread->last_exec_time = time_stamp();
        if(temp != NULL) {
            swapcontext(&(temp->uc), &(current_thread->uc));
        }
        else {
            //main context -> only enters here for first call
            swapcontext(&uc_temp, &(current_thread->uc));
        }
    }
}

void scheduler_init() {
    mlfq = malloc(LEVELS * sizeof(queue));
    if(mlfq == NULL) {
        printf("error: malloc failed for mlfq\n");
    }
    wait = malloc(sizeof(queue));
    if(wait == NULL) {
        printf("error: malloc failed for wait queue\n");
    }
    main_thread = malloc(sizeof(thread_t));
    if(main_thread == NULL) {
        printf("error: malloc failed for main thead\n");
    }

    main_thread->id = 0;
    main_thread->next = main_thread;
    number_of_threads = 0;
    current_thread = NULL;
    
    int i;
    for(i = 0; i < LEVELS; i++) {
        queue_init(mlfq + i);
    }
    printf("mlfq init complete\n");
    queue_init(wait);
    printf("wait queue init complete\n");

    signal(SIGALRM, scheduler);
    printf("signal handler registered\n");
    scheduler();
}

void scheduler_add_thread(thread_t *thread, int priority) {
    thread->thread_state = READY;
    thread->running_time = 0;
    thread->priority = priority;
    enqueue(&(mlfq[priority]), thread);
    number_of_threads++;
}

thread_t* scheduler_get_next_thread() {
    for(int i = 0; i < LEVELS; i++) {
        if((mlfq+i)->front != NULL) {
            thread_t *temp;
            temp = dequeue(mlfq+i);
            number_of_threads--;
            return temp;
        }
    }
    printf("no threads in scheduler\n");
    return NULL;
}

//threading funcs

//wrapper to execute function associated with a thread
void thread_runner(thread_t *thread, void *(*function)(void *), void *arg) {
    thread->thread_state = RUNNING;
    current_thread = thread;
    thread->return_val = function(arg);
    thread->thread_state = DONE;
    scheduler();
}

int thread_create(thread_t *thread, pthread_attr_t *attr, void *(*function)(void *), void *arg) {
    //run scheduler_init if this is the first thread, check first_thread flag
    if(first_thread == 0) {
        scheduler_init();
        printf("scheduler init\n");
        first_thread = 1;
    }
    thread->id = thread_counter++;
    if(getcontext(&(thread->uc)) != 0){
        printf("error: getting context failed\n");
        exit(1);
    }
    thread->uc.uc_link = NULL;
    thread->uc.uc_stack.ss_sp = malloc(MEM);
    if(thread->uc.uc_stack.ss_sp == 0) {
        printf("error: malloc for ucontext stack failed\n");
        exit(1);
    }
    thread->uc.uc_stack.ss_size = MEM;
    thread->uc.uc_stack.ss_flags = 0;
    thread->return_val = NULL;

    //associate input thread to thread_runner, which is used to execute the function passed as arg
    makecontext(&(thread->uc), (void *)thread_runner, 3, thread, function, arg);
    scheduler_add_thread(thread, 0);
    printf("thread with id: %ld added to scheduler\n", thread->id);

    return 0;
}

void thread_yield() {
    current_thread->thread_state = YIELDED;
    scheduler();
}

//exits current_thread, save return val from value_ptr in current_thread->return_val
//if exit is called then function did not return anything, either exit is called or return executes
//successfully
void thread_exit(void *value_ptr) {
    if(current_thread->thread_state != DONE) {
        current_thread->thread_state = DONE;
        current_thread->return_val = value_ptr;
        scheduler();
    }
}

//yield current thread until referenced thread is DONE, save return val from value_ptr in
//current_thread->return_val
int thread_join(thread_t *thread, void **value_ptr) {
    while(thread->thread_state != DONE) {
        thread_yield();
    }
    thread->return_val = value_ptr;
}

//mutex funcs

int thread_mutex_init(thread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
    if(mutex == NULL) {
        printf("error: mutex pointer to NULL\n");
        return 1;
    }
    mutex->lock = 1;
    mutex->owner = NULL;
    mutex->wait = malloc(sizeof(queue));
    queue_init(mutex->wait);

    return 0;
}

//__sync_val_compare_and_swap performs an atomic compare and swap, ie, if the current value of the arg is x,
//then write the new val y into it.
int thread_mutex_lock(thread_mutex_t *mutex) {
    //spin till lock is obtained
    while(__sync_val_compare_and_swap(&(mutex->lock), 0, 1) != 0) {
        current_thread->thread_state = WAITING;
        printf("current thread has to wait for mutex, adding to wait queue\n");
        enqueue(mutex->wait, current_thread);
        scheduler();
    }
    mutex->owner = current_thread;
}

int thread_mutex_unlock(thread_mutex_t *mutex) {
    thread_t *temp;
    if(mutex->wait->front != NULL) {
        temp = dequeue(mutex->wait);
        printf("dequeued from wait queue and adding to mlfq for processing\n");
        scheduler_add_thread(temp, temp->priority);
    }
    mutex->lock = 0;
    printf("mutex now available\n");
}

int thread_mutex_destroy(thread_mutex_t *mutex) {
    if(mutex == NULL) {
        printf("error: mutex pointer to null\n");
        return 1;
    }
    if(mutex->lock != 0) {
        printf("error: mutex is locked\n");
        return 1;
    }
    free(mutex);
    return 0;
}

//Test funcs -> not my implementation

void f0(int arr[])
{
   printf("insertion sort started\n");
   int i, key, j, n;
   n = sizeof(arr)/sizeof(int);
   for (i = 1; i < n; i++)
   {
       key = arr[i];
       j = i-1;
 
       while (j >= 0 && arr[j] > key)
       {
           arr[j+1] = arr[j];
           j = j-1;
       }
       arr[j+1] = key;
   }
   printf("largest element: %d\n", arr[n-1]);
}

void f1_with_mutex() 
{
	printf("Function Entry : f1_with_mutex\n");
    int i=0;
    int local_var;    

    thread_mutex_lock(m);
    local_var = var_w_mutex;
    printf("f1_with_mutex: reading var_w_mutex, the value is %d\n", local_var);
    
    while(i<1234)
    	i++;
    
    local_var = local_var + 100;
    var_w_mutex = local_var;
    printf("f1_with_mutex: writing var_w_mutex, the value now is %d\n", var_w_mutex);
    thread_mutex_unlock(m);

    printf("Function Exit : f1_with_mutex\n");
}

void f2_with_mutex() 
{
	printf("Function Entry : f2_with_mutex\n");
    long int i=0;  
    int local_var;    

    thread_mutex_lock(m);
    local_var = var_w_mutex;
    printf("f2_with_mutex: reading var_w_mutex, the value is %d\n", local_var);
    
    while(i<1234)
    	i++;
    
    local_var = local_var - 50;
    var_w_mutex = local_var;
    printf("f2_with_mutex: writing var_w_mutex, the value now is %d\n", var_w_mutex);
    thread_mutex_unlock(m);
    
    value_ptr = var_w_mutex;
    thread_mutex_destroy(m);
    printf("Function Exit : f2_with_mutex\n");
}

void f1_without_mutex() 
{
	printf("Function Entry : f1_without_mutex\n");
    long int i=0;
    int local_var;   
    local_var = var_wo_mutex;
    printf("f1_without_mutex: reading var_wo_mutex, the value is %d\n", local_var);
    
    while(i<123456789)
    	i++;
    
    local_var = local_var + 100;
    var_wo_mutex = local_var;
    printf("f1_without_mutex: writing var=1, the value now is %d\n", var_wo_mutex);
    printf("Function Exit : f1_without_mutex\n");
}

void f2_without_mutex() 
{  
	printf("Function Entry : f2_without_mutex\n");
    long int i=0;  
    int local_var;     
    local_var = var_wo_mutex;
    printf("f2_without_mutex: read the var_wo_mutex, the value is %d\n", local_var);
    
    while(i<1234)
    	i++;
    
    local_var = local_var - 50;
    var_wo_mutex = local_var;
    printf("f2_without_mutex: writing var_wo_mutex, the value now is %d\n", var_wo_mutex);
    printf("Function Exit : f2_without_mutex\n");
}

void f3() 
{
	printf("Function Entry : f3\n"); 
    long int i=0;   
    FILE *fp;  
    fp=fopen("f0.dat", "w"); 
    printf("writing to a file\n");
    while(i<1234)
    {
    	fprintf(fp, "%d", 1); //Very very slow operation, is evident as well. Finishes at the last.
    	i++;
    }

    fflush(fp);
    printf("writing to file complete\n");
    fclose(fp); 
	printf("Function Exit : f3\n");
}

void f4() 
{
	printf("Function Entry : f4\n");
	int i;
	for(i = 10; i < 30; i++) 
		printf("i: %d\n", i);
	//thread_t_join(&threads[7], NULL);
	printf("Function Exit : f4\n");
}

void f5() 
{
	printf("Function Entry : f5\n");
	int i;
	for(i = 30; i < 50; i++) 
	{
		printf("i: %d\n", i);
		if (i == 39) 
		{
			value_ptr = 10;
			printf("exit called here\n");
			thread_exit(NULL);
		}
	}
	printf("Function Exit : f5\n");
}

void f6() 
{
	printf("Function Entry : f6\n");
	int i;
	for(i = 50; i < 70; i++) 
	{
		printf("i: %d\n", i);
		if (i == 59 || i == 60) 
		{
			thread_yield();
		}
	}
	printf("Function Exit : f6\n");
}

void f7() 
{
	printf("Function Entry : f7\n");
	int i;
	for(i = 70; i < 90; i++) 
	{
		printf("i: %d\n", i);
		if (i == 79) 
		{
			value_ptr = 10;
			//thread_t_join(&threads[7],NULL); //Waiting for f2_with_mutex() to finish
			thread_join(&threads[7],value_ptr);
		}
	}
	printf("value_ptr:%d\n", value_ptr);
	printf("Function Exit : f7\n");
}

int main() {
    threads = malloc(NUM_THREADS * sizeof(thread_t));
    if(threads == NULL) {
        printf("error: malloc for threads failed\n");
    }
    m = malloc(sizeof(thread_mutex_t));
    if(m == NULL) {
        printf("error: malloc for mutex failed\n");
    }
    int m_ret = thread_mutex_init(m, NULL);
    if(!m_ret) {
        printf("mutex initialised\n");
    }

    int i;
    long int arr[1000000];
    for(i = 0; i<1000000; i++) {
        arr[i] = 1000000 - i;
    }

    for(i = 0; i < NUM_THREADS - 9; i++) {
		arr[i] = 1000000 - i;
	}	

	for (i = 0; i < NUM_THREADS-9; i++) 
	{
		if (thread_create(&threads[i], NULL, (void *(*)(void *))f0, (void *)arr) == 1)
			printf("error: Creating Thread %d\n", i);
	}
	if (thread_create(&threads[NUM_THREADS-9], NULL, (void *(*)(void *))f1_with_mutex, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-9);
	if (thread_create(&threads[NUM_THREADS-8], NULL, (void *(*)(void *))f2_with_mutex, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-8);
	if (thread_create(&threads[NUM_THREADS-7], NULL, (void *(*)(void *))f1_without_mutex, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-7);
	if (thread_create(&threads[NUM_THREADS-6], NULL, (void *(*)(void *))f2_without_mutex, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-6);
	if (thread_create(&threads[NUM_THREADS-5], NULL, (void *(*)(void *))f3, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-5);
	if (thread_create(&threads[NUM_THREADS-4], NULL, (void *(*)(void *))f4, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-4);
	if (thread_create(&threads[NUM_THREADS-3], NULL, (void *(*)(void *))f5, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-3);
	if (thread_create(&threads[NUM_THREADS-2], NULL, (void *(*)(void *))f6, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-2);
	if (thread_create(&threads[NUM_THREADS-1], NULL, (void *(*)(void *))f7, NULL) == 1)
			printf("error: creating Thread %d\n", NUM_THREADS-1);
	while(1);

	return 0;
}
