
#include <iostream>
#include <thread>
#include <chrono>

#include "common.h"
#include "logger.h"
#include "shared_counter.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        return 1;
    }

    std::string worker_type = argv[1];

    if (!init_shared_counter())
    {
        std::cerr << "Failed to connect to shared counter.\n";
        return 1;
    }

    pid_t pid = get_pid();

    write_log("PID=" + std::to_string(pid) +
              " " + worker_type + " START");

    if (worker_type == "worker1")
    {
        int result = increment_counter(10);

        write_log("PID=" + std::to_string(pid) +
                  " COUNTER +10 => " + std::to_string(result));
    }
    else if (worker_type == "worker2")
    {
        int result = multiply_counter(2);

        write_log("PID=" + std::to_string(pid) +
                  " COUNTER *2 => " + std::to_string(result));

        std::this_thread::sleep_for(std::chrono::seconds(2));

        result = divide_counter(2);

        write_log("PID=" + std::to_string(pid) +
                  " COUNTER /2 => " + std::to_string(result));
    }
    else
    {
        close_shared_counter();
        return 1;
    }

    write_log("PID=" + std::to_string(pid) +
              " " + worker_type + " EXIT");

    close_shared_counter();

    return 0;
}
