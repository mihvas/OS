
#ifndef COMMON_H
#define COMMON_H

#include <chrono>
#include <ctime>
#include <cstdio>
#include <string>

#define COUNTER_INTERVAL_MS 300
#define LOG_INTERVAL_MS 1000
#define SPAWN_INTERVAL_MS 3000

#ifdef _WIN32

#include <windows.h>

using ProcessId = DWORD;

inline int* shared_counter = nullptr;
inline HANDLE counter_semaphore = nullptr;
inline HANDLE shared_counter_mapping = nullptr;

#else

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <semaphore.h>
#include <cerrno>

using ProcessId = pid_t;

inline int* shared_counter = nullptr;
inline sem_t* counter_semaphore = SEM_FAILED;

#endif

inline std::string LOG_FILE;

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
    std::string log_message =
        "[" + get_time() + "] " + message + "\n";

#ifdef _WIN32
    HANDLE file = CreateFileA(
        LOG_FILE.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file == INVALID_HANDLE_VALUE)
    {
        std::cerr << "CreateFileA failed: " << GetLastError() << std::endl;
        return;
    }

    OVERLAPPED overlapped{};

    if (!LockFileEx(
            file,
            LOCKFILE_EXCLUSIVE_LOCK,
            0,
            1,
            0,
            &overlapped))
    {
        std::cerr << "LockFileEx failed: " << GetLastError() << std::endl;
        CloseHandle(file);
        return;
    }

    LARGE_INTEGER position{};
    position.QuadPart = 0;

    SetFilePointerEx(
        file,
        position,
        nullptr,
        FILE_END
    );

    DWORD written = 0;

    if (!WriteFile(
            file,
            log_message.c_str(),
            static_cast<DWORD>(log_message.size()),
            &written,
            nullptr))
    {
        std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
    }

    UnlockFileEx(
        file,
        0,
        1,
        0,
        &overlapped
    );

    CloseHandle(file);


#else

    int fd = open(LOG_FILE.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);

    if (fd == -1)
        return;

    flock(fd, LOCK_EX);

    write(fd, log_message.c_str(), log_message.size());

    flock(fd, LOCK_UN);

    close(fd);

#endif
}

#ifdef _WIN32


inline bool init_shared_counter(
    const std::string& shm_name,
    const std::string& sem_name
)
{
    counter_semaphore = CreateSemaphoreA(
        nullptr,
        1,
        1,
        sem_name.c_str()
    );

    if (counter_semaphore == nullptr)
        return false;

    DWORD semaphore_result = WaitForSingleObject(
        counter_semaphore,
        INFINITE
    );

    if (semaphore_result != WAIT_OBJECT_0)
    {
        CloseHandle(counter_semaphore);
        counter_semaphore = nullptr;
        return false;
    }

    shared_counter_mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(int),
        shm_name.c_str()
    );

    if (shared_counter_mapping == nullptr)
    {
        ReleaseSemaphore(counter_semaphore, 1, nullptr);
        CloseHandle(counter_semaphore);
        counter_semaphore = nullptr;
        return false;
    }

    bool created = GetLastError() != ERROR_ALREADY_EXISTS;

    shared_counter = static_cast<int*>(
        MapViewOfFile(
            shared_counter_mapping,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(int)
        )
    );

    if (shared_counter == nullptr)
    {
        ReleaseSemaphore(counter_semaphore, 1, nullptr);

        CloseHandle(shared_counter_mapping);
        CloseHandle(counter_semaphore);

        shared_counter_mapping = nullptr;
        counter_semaphore = nullptr;

        return false;
    }

    if (created)
        *shared_counter = 0;

    ReleaseSemaphore(counter_semaphore, 1, nullptr);

    return true;
}

#else

inline bool init_shared_counter(
    const std::string& shm_name,
    const std::string& sem_name
)
{
    counter_semaphore = sem_open(sem_name.c_str(), O_CREAT, 0666, 1);

    if (counter_semaphore == SEM_FAILED)
        return false;

    sem_wait(counter_semaphore);

    bool created = false;

    int shm_fd = shm_open(
        shm_name.c_str(),
        O_CREAT | O_EXCL | O_RDWR,
        0666
    );

    if (shm_fd != -1)
    {
        created = true;
    }
    else if (errno == EEXIST)
    {
        shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0666);
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
            shm_unlink(shm_name.c_str());
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
            shm_unlink(shm_name.c_str());

        return false;
    }

    if (created)
        *shared_counter = 0;

    sem_post(counter_semaphore);

    return true;
}

#endif


inline void close_shared_counter()
{
#ifdef _WIN32

    if (shared_counter != nullptr)
    {
        UnmapViewOfFile(shared_counter);
        shared_counter = nullptr;
    }

    if (shared_counter_mapping != nullptr)
    {
        CloseHandle(shared_counter_mapping);
        shared_counter_mapping = nullptr;
    }

    if (counter_semaphore != nullptr)
    {
        CloseHandle(counter_semaphore);
        counter_semaphore = nullptr;
    }

#else

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

#endif
}


inline int increment_counter(int value)
{
#ifdef _WIN32

    WaitForSingleObject(counter_semaphore, INFINITE);

    *shared_counter += value;

    int result = *shared_counter;

    ReleaseSemaphore(counter_semaphore, 1, nullptr);

    return result;

#else

    sem_wait(counter_semaphore);

    *shared_counter += value;

    int result = *shared_counter;

    sem_post(counter_semaphore);

    return result;

#endif
}


inline int get_counter()
{
#ifdef _WIN32

    WaitForSingleObject(counter_semaphore, INFINITE);

    int result = *shared_counter;

    ReleaseSemaphore(counter_semaphore, 1, nullptr);

    return result;

#else

    sem_wait(counter_semaphore);

    int result = *shared_counter;

    sem_post(counter_semaphore);

    return result;

#endif
}


inline int set_counter(int value)
{
#ifdef _WIN32

    WaitForSingleObject(counter_semaphore, INFINITE);

    *shared_counter = value;

    ReleaseSemaphore(counter_semaphore, 1, nullptr);

    return value;

#else

    sem_wait(counter_semaphore);

    *shared_counter = value;

    sem_post(counter_semaphore);

    return value;

#endif
}


inline int multiply_counter(int value)
{
#ifdef _WIN32

    WaitForSingleObject(counter_semaphore, INFINITE);

    *shared_counter *= value;

    int result = *shared_counter;

    ReleaseSemaphore(counter_semaphore, 1, nullptr);

    return result;

#else

    sem_wait(counter_semaphore);

    *shared_counter *= value;

    int result = *shared_counter;

    sem_post(counter_semaphore);

    return result;

#endif
}


inline int divide_counter(int value)
{
    if (value == 0)
        return -1;

#ifdef _WIN32

    WaitForSingleObject(counter_semaphore, INFINITE);

    *shared_counter /= value;

    int result = *shared_counter;

    ReleaseSemaphore(counter_semaphore, 1, nullptr);

    return result;

#else

    sem_wait(counter_semaphore);

    *shared_counter /= value;

    int result = *shared_counter;

    sem_post(counter_semaphore);

    return result;

#endif
}

#endif
