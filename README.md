# ThreadSchedulingC

## Implementing the functioning of a multi-level feedback queue and wait queue with User Threads

Specs:
* Possible thread states: READY, RUNNING, WAITING, YIELDED, DONE
* Scheduler run every 50ms
* 16 MLFQ queues (Varying levels/priorities)
* Time slice for each level: (level + 1) * 50ms
* Starvation is checked after ever 50 time slices (QUANT)

Working:
* The scheduler gets initialised when the first thread is created
* The next thread is fetched for execution after enqueueing the current thread into one of the queues
* Starvation is checked in all threads in the MLFQ, enqueue starved threads into the queue with the highest priority
* Swap context to current thread and proceed with the execution after this step

Testing:
* Implemented test funcs that are run by the threads, these functions increment, decrement values with or without holding a mutex.
* Also implemented test funcs to test thread_yield, thread_exit, thread_join

Usage:
* run ```gcc thread.c``` and ```./a.out``` to run the prewritten tests
