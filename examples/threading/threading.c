#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    usleep(thread_func_args->wait_to_obtain_ms * 1000);
    printf("Obtaining Lock\n");
    if (pthread_mutex_lock(thread_func_args->mutex) != 0)
    {
        perror("Failed In Obtaining the Mutex\n");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    printf("Lock Obtained\n");
    usleep(thread_func_args->wait_to_release_ms * 1000);

    printf("Lock Releasing\n");
    if (pthread_mutex_unlock(thread_func_args->mutex)) 
    {
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }
    printf("Lock Released\n");

    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data * data = malloc(sizeof(struct thread_data));
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;

    printf("Thread Creating\n");
    int ret = pthread_create(thread, NULL, threadfunc, data);
    if (ret != 0) 
    {
        free(data);
        return false;
    }
    return true;
}

