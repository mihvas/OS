#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    int exitCode = 0;
    int duration = 3;

    if (argc > 1) {
        duration = std::stoi(argv[1]);
    }

    if (argc > 2) {
        exitCode = std::stoi(argv[2]);
    }

    std::cout << "Child process started." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(duration));

    std::cout << "Child process finished." << std::endl;

    return exitCode;
}