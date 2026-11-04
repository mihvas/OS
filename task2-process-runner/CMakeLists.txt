cmake_minimum_required(VERSION 3.10)

project(ProcessRunner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(process_runner STATIC
    src/process_runner.cpp
)

target_include_directories(process_runner PUBLIC
    include
)

add_executable(process_test
    tests/main.cpp
)

target_link_libraries(process_test PRIVATE
    process_runner
)

add_executable(child
    tests/child.cpp
)