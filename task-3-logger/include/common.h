#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <unistd.h>

#define SHM_NAME "/process_counter_shared_memory"
#define SEM_NAME "/process_counter_semaphore"
#define LOCK_FILE "/tmp/process_counter_coordinator.lock"

constexpr int COUNTER_INTERVAL_MS = 300;
constexpr int LOG_INTERVAL_MS = 1000;
constexpr int SPAWN_INTERVAL_MS = 3000;

inline pid_t get_pid()
{
    return getpid();
}

inline std::string get_current_time()
{
    auto now = std::chrono::system_clock::now();

    auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;

    std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};

    localtime_r(&time, &tm);

    std::ostringstream stream;

    stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
          << "."
          << std::setfill('0')
          << std::setw(3)
          << milliseconds.count();

    return stream.str();
}

#endif