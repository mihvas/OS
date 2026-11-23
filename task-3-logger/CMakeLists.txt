
cmake_minimum_required(VERSION 3.16)

project(ProcessCounter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include_directories(includes)

add_executable(main
    src/main.cpp
)

add_executable(worker
    src/worker.cpp
)

target_link_libraries(main PRIVATE Threads::Threads)
target_link_libraries(worker PRIVATE Threads::Threads)


# cmake -S . -B build -G "MinGW Makefiles"
# cmake --build build