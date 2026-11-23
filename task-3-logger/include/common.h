
#ifndef COMMON_H
#define COMMON_H

#include <chrono>
#include <ctime>
#include <cstdio>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <semaphore.h>
#include <cerrno>

#define SHM_NAME "/process_counter_shm"
#define SEM_NAME "/process_counter_sem"

#define LOG_FILE "process.log"

inline int* shared_counter = nullptr;
inline sem_t* counter_semaphore = SEM_FAILED;

inline std::string get_time()
{
    auto now = std::chrono::system_clock::now();

    auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count() % 1000;

    auto time = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    char buffer[32];

    std::strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &tm
    );

    char result[40];

    std::snprintf(
        result,
        sizeof(result),
        "%s.%03lld",
        buffer,
        static_cast<long long>(milliseconds)
    );

    return result;
}



inline void write_log(const std::string& message)
{
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);

    if (fd == -1)
        return;

    flock(fd, LOCK_EX);

    std::string log_message =
        "[" + get_time() + "] " + message + "\n";

    write(fd, log_message.c_str(), log_message.size());

    flock(fd, LOCK_UN);

    close(fd);
}



inline bool init_shared_counter()
{
    counter_semaphore = sem_open(SEM_NAME, O_CREAT, 0666, 1);

    if (counter_semaphore == SEM_FAILED)
        return false;

    sem_wait(counter_semaphore);

    bool created = false;

    int shm_fd = shm_open(
        SHM_NAME,
        O_CREAT | O_EXCL | O_RDWR,
        0666
    );

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
        if (ftruncate(shm_fd, sizeof(int)) == -1)
        {
            close(shm_fd);
            sem_post(counter_semaphore);
            sem_close(counter_semaphore);
            counter_semaphore = SEM_FAILED;
            shm_unlink(SHM_NAME);
            return false;
        }
    }

    shared_counter = static_cast<int*>(
        mmap(
            nullptr,
            sizeof(int),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            shm_fd,
            0
        )
    );

    close(shm_fd);

    if (shared_counter == MAP_FAILED)
    {
        shared_counter = nullptr;
        sem_post(counter_semaphore);
        sem_close(counter_semaphore);
        counter_semaphore = SEM_FAILED;

        if (created)
            shm_unlink(SHM_NAME);

        return false;
    }

    if (created)
        *shared_counter = 0;

    sem_post(counter_semaphore);

    return true;
}


inline void close_shared_counter()
{
    if (shared_counter != nullptr)
    {
        munmap(shared_counter, sizeof(int));
        shared_counter = nullptr;
    }

    if (counter_semaphore != SEM_FAILED)
    {
        sem_close(counter_semaphore);
        counter_semaphore = SEM_FAILED;
    }
}


inline int increment_counter(int value)
{
    sem_wait(counter_semaphore);

    *shared_counter += value;

    int result = *shared_counter;

    sem_post(counter_semaphore);

    return result;
}


inline int get_counter()
{
    sem_wait(counter_semaphore);

    int result = *shared_counter;

    sem_post(counter_semaphore);

    return result;
}


inline int set_counter(int value)
{
    sem_wait(counter_semaphore);

    *shared_counter = value;

    sem_post(counter_semaphore);

    return value;
}


inline int multiply_counter(int value)
{
    sem_wait(counter_semaphore);

    *shared_counter *= value;

    int result = *shared_counter;

    sem_post(counter_semaphore);

    return result;
}


inline int divide_counter(int value)
{
    if (value == 0)
        return -1;

    sem_wait(counter_semaphore);

    *shared_counter /= value;

    int result = *shared_counter;

    sem_post(counter_semaphore);

    return result;
}

#endif