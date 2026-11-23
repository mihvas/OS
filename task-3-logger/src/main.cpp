#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <unistd.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <cstdlib>

#include "common.h"

using Clock = std::chrono::steady_clock;

pid_t worker1_pid = -1;
pid_t worker2_pid = -1;

std::string worker_path;
std::string shm_name;
std::string sem_name;
std::string lock_path;

std::string get_executable_path()
{
    char path[4096];

    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (length == -1)
        return "";

    path[length] = '\0';

    return path;
}

bool init_paths()
{
    std::string executable_path = get_executable_path();

    if (executable_path.empty())
        return false;

    std::filesystem::path executable(executable_path);
    std::filesystem::path directory = executable.parent_path();

    worker_path = (directory / "worker").string();

    LOG_FILE = (directory / "process.log").string();
    lock_path = (directory / "process.lock").string();

    std::string directory_name = directory.string();

    for (char& c : directory_name)
    {
        if (c == '/')
            c = '_';
    }

    shm_name = "/process_counter_shm_" + directory_name;
    sem_name = "/process_counter_sem_" + directory_name;

    return true;
}

bool process_finished(pid_t& pid)
{
    if (pid <= 0)
        return true;

    int status;

    pid_t result = waitpid(pid, &status, WNOHANG);

    if (result == pid || (result == -1))
    {
        pid = -1;
        return true;
    }

    return false;
}

pid_t start_worker(const std::string& type)
{
    pid_t pid = fork();

    if (pid == -1)
    {
        write_log("Failed to fork " + type);
        return -1;
    }

    if (pid == 0)
    {
        execl(
            worker_path.c_str(),
            worker_path.c_str(),
            type.c_str(),
            shm_name.c_str(),
            sem_name.c_str(),
            LOG_FILE.c_str(),
            nullptr
        );

        _exit(127);
    }

    return pid;
}

bool workers_finished()
{
    bool first_finished = process_finished(worker1_pid);
    bool second_finished = process_finished(worker2_pid);

    return first_finished && second_finished;
}

void process_command(const std::string& command)
{
    try
    {
        size_t position = 0;

        int value = std::stoi(command, &position);

        if (position != command.size())
        {
            std::cout << "Invalid number.\n";
            return;
        }

        set_counter(value);

        std::cout << "Counter set to " << get_counter() << '\n';

        write_log("Counter manually changed to " +
                  std::to_string(value));
    }
    catch (...)
    {
        std::cout << "Invalid number.\n";
    }
}

int main()
{
    if (!init_paths())
    {
        std::cerr << "Failed to determine executable path.\n";
        return 1;
    }

    if (!init_shared_counter(shm_name, sem_name))
    {
        std::cerr << "Failed to initialize shared counter.\n";
        return 1;
    }

    int lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0666);

    if (lock_fd == -1)
    {
        std::cerr << "Failed to open coordinator lock file.\n";
        close_shared_counter();
        return 1;
    }

    pid_t pid = getpid();

    std::cout << "Enter a number to change the counter.\n";

    auto next_increment = Clock::now();
    auto next_log = Clock::now() + std::chrono::seconds(1);
    auto next_spawn = Clock::now() + std::chrono::seconds(3);

    bool coordinator = false;

    std::string input;

    if (flock(lock_fd, LOCK_EX | LOCK_NB) == 0)
    {
        coordinator = true;

        set_counter(0);

        write_log("PID=" + std::to_string(pid) +
                " Became coordinator");

        std::cout << "This process is coordinator.\n";
    }

    while (true)
    {
        auto now = Clock::now();

        if (!coordinator)
        {
            if (flock(lock_fd, LOCK_EX | LOCK_NB) == 0)
            {
                coordinator = true;
                
                next_increment = Clock::now();
                next_log = Clock::now() + std::chrono::seconds(1);
                next_spawn = Clock::now() + std::chrono::seconds(3);

                write_log("PID=" + std::to_string(pid) +
                          " Became coordinator");

                std::cout << "This process is coordinator.\n";
            }
        }

        if (now >= next_increment)
        {
            increment_counter(1);

            next_increment += std::chrono::milliseconds(COUNTER_INTERVAL_MS);
        }

        if (coordinator && now >= next_log)
        {
            write_log("PID=" + std::to_string(pid) +
                      " COUNTER=" + std::to_string(get_counter()));

            next_log += std::chrono::milliseconds(LOG_INTERVAL_MS);
        }

        if (coordinator && now >= next_spawn)
        {
            if (workers_finished())
            {
                worker1_pid = start_worker("worker1");
                worker2_pid = start_worker("worker2");

                write_log("PID=" + std::to_string(pid) +
                          " Started workers");
            }
            else
            {
                write_log("PID=" + std::to_string(pid) +
                          " Workers still running.");
            }

            next_spawn += std::chrono::milliseconds(SPAWN_INTERVAL_MS);
        }

        pollfd pfd{};

        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        int result = poll(&pfd, 1, 50);

        if (result > 0 && (pfd.revents & POLLIN))
        {
            if (!std::getline(std::cin, input))
                break;

            process_command(input);
        }
    }

    write_log("PID=" + std::to_string(pid) +
              "Coordinator shutdown");

    if (worker1_pid > 0)
    {
        kill(worker1_pid, SIGTERM);
        waitpid(worker1_pid, nullptr, 0);
        worker1_pid = -1;
    }

    if (worker2_pid > 0)
    {
        kill(worker2_pid, SIGTERM);
        waitpid(worker2_pid, nullptr, 0);
        worker2_pid = -1;
    }

    if (coordinator)
        flock(lock_fd, LOCK_UN);

    close(lock_fd);

    close_shared_counter();

    return 0;
}
