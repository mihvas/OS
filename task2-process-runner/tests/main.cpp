#include "process_runner.h"

#include <iostream>
#include <thread>
#include <chrono>

int main() {
#ifdef _WIN32
    const std::string childProgram = "child.exe";
#else
    const std::string childProgram = "./child";
#endif

    // Test 1: Start and wait for a process
    std::cout << "Test 1: Start process" << std::endl;

    ProcessId process = start_process(childProgram, {"2", "7"});

#ifdef _WIN32
    if (process == nullptr) {
#else
    if (process < 0) {
#endif
        std::cerr << "Failed to start process." << std::endl;
        return 1;
    }

    std::cout << "Process started." << std::endl;

    std::cout << "Is running: "
              << (is_process_running(process) ? "yes" : "no")
              << std::endl;

    int exitCode = wait_process(process);

    std::cout << "Process finished with code: "
              << exitCode << std::endl;

    if (exitCode != 7) {
        std::cerr << "Test 1 failed." << std::endl;
        return 1;
    }

    // Test 2: Kill a process
    std::cout << "\nTest 2: Kill process" << std::endl;

    process = start_process(childProgram, {"10", "0"});

#ifdef _WIN32
    if (process == nullptr) {
#else
    if (process < 0) {
#endif
        std::cerr << "Failed to start process." << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Is running: "
              << (is_process_running(process) ? "yes" : "no")
              << std::endl;

    if (!kill_process(process)) {
        std::cerr << "Failed to kill process." << std::endl;
        wait_process(process);
        return 1;
    }

    std::cout << "Process killed." << std::endl;

    exitCode = wait_process(process);

    std::cout << "Exit code after killing: "
              << exitCode << std::endl;

    std::cout << "\nAll tests completed." << std::endl;

    return 0;
}