
#include "process_runner.h"

#include <iostream>
#include <thread>
#include <chrono>

int main() {
#ifdef _WIN32
    const std::string childProgram = "build/child.exe";
#else
    const std::string childProgram = "./build/child";
#endif

    ProcessId process;
    int exitCode;

    // Test 1: Start and wait for a process
    std::cout << "Test 1: Start process" << std::endl;

    process = start_process(childProgram, {"2", "7"});

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

    exitCode = wait_process(process);

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

#ifdef _WIN32
    if (exitCode != 1) {
#else
    if (exitCode != 137) {
#endif
        std::cerr << "Test 2 failed." << std::endl;
        return 1;
    }

    // Test 3: Start a nonexistent program
    std::cout << "\nTest 3: Start nonexistent program" << std::endl;

    process = start_process("definitely_not_a_real_program_12345", {});

#ifdef _WIN32
    if (process != nullptr) {
#else
    if (process >= 0) {
#endif
        std::cerr << "Test 3 failed: program should not start." << std::endl;
        return 1;
    }

    std::cout << "Program was not started, as expected." << std::endl;

    // Test 4: Process exits with code 127
    std::cout << "\nTest 4: Exit code 127" << std::endl;

    process = start_process(childProgram, {"0", "127"});

#ifdef _WIN32
    if (process == nullptr) {
#else
    if (process < 0) {
#endif
        std::cerr << "Test 4 failed: could not start child." << std::endl;
        return 1;
    }

    exitCode = wait_process(process);

    std::cout << "Process finished with code: "
              << exitCode << std::endl;

    if (exitCode != 127) {
        std::cerr << "Test 4 failed." << std::endl;
        return 1;
    }

    std::cout << "\nAll tests passed." << std::endl;

    return 0;
}
