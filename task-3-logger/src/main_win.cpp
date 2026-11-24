
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <windows.h>
#include <conio.h>

#include "common.h"

using Clock = std::chrono::steady_clock;

ProcessId worker1_pid = 0;
ProcessId worker2_pid = 0;

HANDLE worker1_handle = nullptr;
HANDLE worker2_handle = nullptr;

std::string worker_path;
std::string shm_name;
std::string sem_name;
std::string lock_path;

std::string get_executable_path()
{
    char path[MAX_PATH];

    DWORD length = GetModuleFileNameA(
        nullptr,
        path,
        MAX_PATH
    );

    if (length == 0 || length == MAX_PATH)
        return "";

    return path;
}


std::string get_windows_object_name(const std::string& name)
{
    std::string result = name;

    for (char& c : result)
    {
        if (c == '/' || c == '\\')
            c = '_';
    }

    return result;
}


bool init_paths()
{
    std::string executable_path = get_executable_path();

    if (executable_path.empty())
        return false;

    std::filesystem::path executable(executable_path);
    std::filesystem::path directory = executable.parent_path();

    worker_path = (directory / "worker.exe").string();

    LOG_FILE = (directory / "process.log").string();
    lock_path = (directory / "process.lock").string();

    std::string directory_name = directory.string();

    shm_name = get_windows_object_name(directory_name);
    sem_name = get_windows_object_name(directory_name);

    shm_name = "ProcessCounter_Shared_" + shm_name;
    sem_name = "ProcessCounter_Semaphore_" + sem_name;

    return true;
}

HANDLE open_lock_file()
{
    return CreateFileA(
        lock_path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
}


bool try_lock(HANDLE lock_handle, OVERLAPPED& overlapped)
{
    return LockFileEx(
        lock_handle,
        LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
        0,
        1,
        0,
        &overlapped
    );
}


void unlock(HANDLE lock_handle, OVERLAPPED& overlapped)
{
    UnlockFileEx(
        lock_handle,
        0,
        1,
        0,
        &overlapped
    );
}

bool process_finished(ProcessId& pid, HANDLE& process_handle)
{
    if (process_handle == nullptr)
        return true;

    DWORD result = WaitForSingleObject(process_handle, 0);

    if (result == WAIT_OBJECT_0)
    {
        CloseHandle(process_handle);

        process_handle = nullptr;
        pid = 0;

        return true;
    }

    return false;
}


std::string quote_argument(const std::string& argument)
{
    std::string result = "\"";

    for (char c : argument)
    {
        if (c == '"')
            result += '\\';

        result += c;
    }

    result += '"';

    return result;
}


ProcessId start_worker(
    const std::string& type,
    HANDLE& process_handle
)
{
    std::string command_line =
        quote_argument(worker_path) + " " +
        quote_argument(type) + " " +
        quote_argument(shm_name) + " " +
        quote_argument(sem_name) + " " +
        quote_argument(LOG_FILE);

    STARTUPINFOA startup_info{};
    startup_info.cb = sizeof(startup_info);

    PROCESS_INFORMATION process_info{};

    BOOL result = CreateProcessA(
        worker_path.c_str(),
        command_line.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startup_info,
        &process_info
    );

    if (!result)
    {
        write_log("Failed to start " + type);
        return 0;
    }

    CloseHandle(process_info.hThread);

    process_handle = process_info.hProcess;

    return process_info.dwProcessId;
}


bool workers_finished()
{
    bool first_finished =
        process_finished(worker1_pid, worker1_handle);

    bool second_finished =
        process_finished(worker2_pid, worker2_handle);

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
            std::cout << "\nInvalid number.\n";
            return;
        }

        set_counter(value);

        std::cout << "\nCounter set to "
                  << get_counter() << '\n';

        write_log(
            "Counter manually changed to " +
            std::to_string(value)
        );
    }
    catch (...)
    {
        std::cout << "\nInvalid number.\n";
    }
}


bool read_input(std::string& input)
{
    if (!_kbhit())
        return false;

    int key = _getch();

    if (key == '\r')
    {
        process_command(input);
        input.clear();

        std::cout << "Enter a number: ";

        return true;
    }

    if (key == '\b')
    {
        if (!input.empty())
        {
            input.pop_back();
            std::cout << "\b \b";
        }

        return true;
    }

    if (key >= 32 && key <= 126)
    {
        input += static_cast<char>(key);
        std::cout << static_cast<char>(key);
    }

    return true;
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

    HANDLE lock_handle = open_lock_file();

    if (lock_handle == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to open coordinator lock file.\n";
        close_shared_counter();
        return 1;
    }

    ProcessId pid = GetCurrentProcessId();

    std::cout << "Enter a number to change the counter.\n";

    auto next_increment = Clock::now();
    auto next_log = Clock::now() + std::chrono::seconds(1);
    auto next_spawn = Clock::now() + std::chrono::seconds(3);

    bool coordinator = false;

    OVERLAPPED lock_overlapped{};

    std::string input;

    if (try_lock(lock_handle, lock_overlapped))
    {
        coordinator = true;

        set_counter(0);

        write_log(
            "PID=" + std::to_string(pid) +
            " Became coordinator"
        );

        std::cout << "\nThis process is coordinator.\n";
    }

    while (true)
    {
        auto now = Clock::now();

        if (!coordinator)
        {
            if (try_lock(lock_handle, lock_overlapped))
            {
                coordinator = true;

                next_log = Clock::now() + std::chrono::seconds(1);
                next_spawn = Clock::now() + std::chrono::seconds(3);

                write_log(
                    "PID=" + std::to_string(pid) +
                    " Became coordinator"
                );

                std::cout << "\nThis process is coordinator.\n";
            }
        }

        if (now >= next_increment)
        {
            increment_counter(1);

            next_increment +=
                std::chrono::milliseconds(COUNTER_INTERVAL_MS);
        }

        if (coordinator && now >= next_log)
        {
            write_log(
                "PID=" + std::to_string(pid) +
                " COUNTER=" + std::to_string(get_counter())
            );

            next_log +=
                std::chrono::milliseconds(LOG_INTERVAL_MS);
        }

        if (coordinator && now >= next_spawn)
        {
            if (workers_finished())
            {
                worker1_pid = start_worker(
                    "worker1",
                    worker1_handle
                );

                worker2_pid = start_worker(
                    "worker2",
                    worker2_handle
                );

                write_log(
                    "PID=" + std::to_string(pid) +
                    " Started workers"
                );
            }
            else
            {
                write_log(
                    "PID=" + std::to_string(pid) +
                    " Workers still running."
                );
            }

            next_spawn +=
                std::chrono::milliseconds(SPAWN_INTERVAL_MS);
        }

        read_input(input);

    }

    write_log(
        "PID=" + std::to_string(pid) +
        " Coordinator shutdown"
    );

    if (worker1_handle != nullptr)
    {
        TerminateProcess(worker1_handle, 1);
        WaitForSingleObject(worker1_handle, INFINITE);
        CloseHandle(worker1_handle);
    }

    if (worker2_handle != nullptr)
    {
        TerminateProcess(worker2_handle, 1);
        WaitForSingleObject(worker2_handle, INFINITE);
        CloseHandle(worker2_handle);
    }

    if (coordinator)
        unlock(lock_handle, lock_overlapped);

    CloseHandle(lock_handle);

    close_shared_counter();

    return 0;
}
