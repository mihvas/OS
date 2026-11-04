cmake_minimum_required(VERSION 3.10)

project(ProcessRunner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(PROCESS_SOURCE src/process_windows.cpp)
else()
    set(PROCESS_SOURCE src/process_linux.cpp)
endif()

add_library(process_runner STATIC
    ${PROCESS_SOURCE}
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