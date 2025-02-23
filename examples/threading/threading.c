#include "threading.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

#define MS_PER_S  (1000)
#define NS_PER_MS (1000 * 1000)

void msleep(int ms)
{
    struct timespec ts = {
        .tv_sec = ms / MS_PER_S,
        .tv_nsec = (ms % MS_PER_S) * NS_PER_MS,
    };

    struct timespec *req = &ts;
    struct timespec *rem = NULL;
    for (;;) {
        switch (nanosleep(req, rem)) {
        case 0:
            return;
        default:
            if (errno == EINTR) {
                req = rem; // interrupted
            } else {
                perror(NULL);
                abort();
            }
        }
    }
}

void* threadfunc(void* thread_param)
{
    struct thread_data *data = thread_param;
    int err;

    msleep(data->wait_to_obtain_ms);

    err = pthread_mutex_lock(data->mutex);
    if (err != 0) {
        ERROR_LOG("failed to lock mutex: %d", err);
        return data;
    }

    msleep(data->wait_to_release_ms);

    err = pthread_mutex_unlock(data->mutex);
    if (err != 0) {
        ERROR_LOG("failed to unlock mutex: %d", err);
        return data;
    }

    data->thread_complete_success = true;
    return data;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *data = malloc(sizeof *data);
    if (data == NULL) {
        perror(NULL);
        abort();
    }
    memset(data, 0, sizeof *data);
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;

    return pthread_create(thread, NULL, threadfunc, data) == 0;
}
