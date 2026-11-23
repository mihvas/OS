
#ifndef SHARED_COUNTER_H
#define SHARED_COUNTER_H

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <cerrno>

#include "common.h"

struct SharedData
{
    int counter;
};

inline SharedData* shared_data = nullptr;
inline sem_t* counter_semaphore = SEM_FAILED;

inline bool init_shared_counter()
{
    counter_semaphore = sem_open(SEM_NAME, O_CREAT, 0666, 1);

    if (counter_semaphore == SEM_FAILED)
    {
        return false;
    }

    sem_wait(counter_semaphore);

    bool created = false;

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);

    if (shm_fd != -1)
    {
        created = true;
    }
    else if (errno == EEXIST)
    {
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    }

    if (shm_fd == -1)
    {
        sem_post(counter_semaphore);
        sem_close(counter_semaphore);
        counter_semaphore = SEM_FAILED;
        return false;
    }

    if (created)
    {
        if (ftruncate(shm_fd, sizeof(SharedData)) == -1)
        {
            close(shm_fd);
            sem_post(counter_semaphore);
            sem_close(counter_semaphore);
            counter_semaphore = SEM_FAILED;
            shm_unlink(SHM_NAME);
            return false;
        }
    }

    shared_data = static_cast<SharedData*>(
        mmap(nullptr,
            sizeof(SharedData),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            shm_fd,
            0)
    );

    close(shm_fd);

    if (shared_data == MAP_FAILED)
    {
        shared_data = nullptr;
        sem_post(counter_semaphore);
        sem_close(counter_semaphore);
        counter_semaphore = SEM_FAILED;
        return false;
    }

    if (created)
    {
        shared_data->counter = 0;
    }

    sem_post(counter_semaphore);

    return true;
}

inline int increment_counter(int value)
{
    sem_wait(counter_semaphore);

    shared_data->counter += value;

    int result = shared_data->counter;

    sem_post(counter_semaphore);

    return result;
}

inline int get_counter()
{
    sem_wait(counter_semaphore);

    int result = shared_data->counter;

    sem_post(counter_semaphore);

    return result;
}

inline int set_counter(int value)
{
    sem_wait(counter_semaphore);

    shared_data->counter = value;

    sem_post(counter_semaphore);

    return value;
}

inline int multiply_counter(int value)
{
    sem_wait(counter_semaphore);

    shared_data->counter *= value;

    int result = shared_data->counter;

    sem_post(counter_semaphore);

    return result;
}

inline int divide_counter(int value)
{
    sem_wait(counter_semaphore);

    shared_data->counter /= value;

    int result = shared_data->counter;

    sem_post(counter_semaphore);

    return result;
}

inline void close_shared_counter()
{
    if (shared_data != nullptr)
    {
        munmap(shared_data, sizeof(SharedData));
        shared_data = nullptr;
    }

    if (counter_semaphore != SEM_FAILED)
    {
        sem_close(counter_semaphore);
        counter_semaphore = SEM_FAILED;
    }
}

#endif
