#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "errno.h"

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // Variables
    int usleep_return;
    int mutex_lock_return;
    int mutex_unlock_return;
    struct thread_data *thread_data_ptr = (struct thread_data *)thread_param;

    // Wait to obtain mutex
    usleep_return = usleep(thread_data_ptr->wait_to_obtain_ms * 1000);
    if (usleep_return == -1)
    {
        ERROR_LOG("Error: Failed to sleep!\n");
        thread_data_ptr->thread_complete_success = false;
        return thread_data_ptr;
    }
    
    // Obtain mutex
    mutex_lock_return = pthread_mutex_lock(thread_data_ptr->mutex_ptr);
    if (mutex_lock_return != 0)
    {
        ERROR_LOG("Error: Failed to lock mutex!\n");
        thread_data_ptr->thread_complete_success = false;
        return thread_data_ptr;
    }

    // Wait to release mutex
    usleep_return = usleep(thread_data_ptr->wait_to_release_ms * 1000);

    // Release mutex
    mutex_unlock_return = pthread_mutex_unlock(thread_data_ptr->mutex_ptr);
    // Only handle sleep error after attempting to unlock mutex
    if (usleep_return == -1)
    {
        ERROR_LOG("Error: Failed to sleep!\n");
        thread_data_ptr->thread_complete_success = false;
        return thread_data_ptr;
    }
    else if (mutex_unlock_return != 0)
    {
        ERROR_LOG("Error: Failed to unlock mutex!\n");
        thread_data_ptr->thread_complete_success = false;
        return thread_data_ptr;
    }

    thread_data_ptr->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    // Variables
    struct thread_data *thread_data_ptr = NULL;
    int pthread_create_return;

    // Thread data
    thread_data_ptr = malloc(sizeof(struct thread_data));
    if (thread_data_ptr == NULL)
    {
        // Memory not allocated
        ERROR_LOG("Error: Failed to allocate memory for thread_data!");
        return false;
    }

    // Setup thread variables
    thread_data_ptr->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data_ptr->wait_to_release_ms = wait_to_release_ms;
    thread_data_ptr->thread_complete_success = false;
    thread_data_ptr->mutex_ptr = mutex;

    // Start thread
    pthread_create_return = pthread_create(thread, NULL, threadfunc, thread_data_ptr);
    if (pthread_create_return != 0)
    {
        // Memory not allocated
        ERROR_LOG("Error: Failed to create thread!");
        free(thread_data_ptr);
        thread_data_ptr = NULL;
        return false;
    }

    return true;
}

