
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

#include "common.h"
#include "logger.h"
#include "shared_counter.h"

using Clock = std::chrono::steady_clock;

pid_t worker1_pid = -1;
pid_t worker2_pid = -1;

std::string worker_path;

// Проверка завершения процесса
bool process_finished(pid_t& pid)
{
    if (pid <= 0)
    {
        return true;
    }

    int status;

    pid_t result = waitpid(pid, &status, WNOHANG);

    if (result == pid)
    {
        pid = -1;
        return true;
    }

    return false;
}

// Запуск worker
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
        execl(worker_path.c_str(),
              worker_path.c_str(),
              type.c_str(),
              nullptr);

        // Если exec не удался
        _exit(127);
    }

    return pid;
}

// Проверка, завершились ли оба работника
bool workers_finished()
{
    bool first_finished = process_finished(worker1_pid);
    bool second_finished = process_finished(worker2_pid);

    return first_finished && second_finished;
}

// Обработка пользовательской команды
void process_command(const std::string& command)
{
    if (command == "show")
    {
        std::cout << "Counter: " << get_counter() << '\n';
    }
    else if (command == "quit")
    {
        std::cout << "Use Ctrl+C to stop the program.\n";
    }
    else if (command.rfind("set ", 0) == 0)
    {
        try
        {
            int value = std::stoi(command.substr(4));

            set_counter(value);

            std::cout << "Counter set to " << value << '\n';
        }
        catch (...)
        {
            std::cout << "Invalid number.\n";
        }
    }
    else
    {
        std::cout << "Unknown command.\n";
    }
}

// Получение пути до worker рядом с main
std::string get_worker_path()
{
    char path[4096];

    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (length == -1)
    {
        return "./worker";
    }

    path[length] = '\0';

    std::filesystem::path executable(path);

    return (executable.parent_path() / "worker").string();
}

int main()
{
    if (!init_shared_counter())
    {
        std::cerr << "Failed to initialize shared counter.\n";
        return 1;
    }

    worker_path = get_worker_path();

    // Блокировка для выбора координатора
    int lock_fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0666);

    if (lock_fd == -1)
    {
        std::cerr << "Failed to open coordinator lock file.\n";
        close_shared_counter();
        return 1;
    }

    bool coordinator = (flock(lock_fd, LOCK_EX | LOCK_NB) == 0);

    pid_t pid = get_pid();

    write_log("PID=" + std::to_string(pid) +
              " MAIN START");

    if (coordinator)
    {
        write_log("PID=" + std::to_string(pid) +
                  " BECAME COORDINATOR");

        std::cout << "This process is coordinator.\n";
    }
    else
    {
        std::cout << "This process is client.\n";
    }

    std::cout << "Commands:\n";
    std::cout << "  set <number> - set counter\n";
    std::cout << "  show         - show counter\n";
    std::cout << "  quit         - show exit instruction\n";
    std::cout << "  Ctrl+C       - exit\n\n";

    auto next_increment = Clock::now();
    auto next_log = Clock::now() + std::chrono::seconds(1);
    auto next_spawn = Clock::now() + std::chrono::seconds(3);

    std::string input;

    while (true)
    {
        auto now = Clock::now();

        // Увеличение счётчика раз в 300 мс
        if (now >= next_increment)
        {
            increment_counter(1);

            next_increment += std::chrono::milliseconds(
                COUNTER_INTERVAL_MS
            );
        }

        // Ежесекундное логирование только координатором
        if (coordinator && now >= next_log)
        {
            write_log("PID=" + std::to_string(pid) +
                      " COUNTER=" + std::to_string(get_counter()));

            next_log += std::chrono::milliseconds(LOG_INTERVAL_MS);
        }

        // Запуск работников каждые 3 секунды только координатором
        if (coordinator && now >= next_spawn)
        {
            if (workers_finished())
            {
                worker1_pid = start_worker("worker1");
                worker2_pid = start_worker("worker2");

                write_log("PID=" + std::to_string(pid) +
                          " STARTED WORKERS");

                std::cout << "Workers started.\n";
            }
            else
            {
                write_log("PID=" + std::to_string(pid) +
                          " WORKERS STILL RUNNING. SKIP");

                std::cout << "Previous workers still running. Skipping.\n";
            }

            next_spawn += std::chrono::milliseconds(
                SPAWN_INTERVAL_MS
            );
        }

        // Если координатор потерял блокировку, он больше не координатор
        if (coordinator)
        {
            if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0)
            {
                coordinator = false;
                write_log("PID=" + std::to_string(pid) +
                          " LOST COORDINATOR");
            }
        }

        // Проверка консольного ввода без блокировки таймеров
        pollfd pfd{};

        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        int result = poll(&pfd, 1, 50);

        if (result > 0 && (pfd.revents & POLLIN))
        {
            if (!std::getline(std::cin, input))
            {
                break;
            }

            process_command(input);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Завершаем оставшихся работников
    if (worker1_pid > 0)
    {
        kill(worker1_pid, SIGTERM);
        waitpid(worker1_pid, nullptr, 0);
    }

    if (worker2_pid > 0)
    {
        kill(worker2_pid, SIGTERM);
        waitpid(worker2_pid, nullptr, 0);
    }

    write_log("PID=" + std::to_string(pid) +
              " MAIN EXIT");

    if (coordinator)
    {
        flock(lock_fd, LOCK_UN);
    }

    close(lock_fd);
    close_shared_counter();

    return 0;
}
