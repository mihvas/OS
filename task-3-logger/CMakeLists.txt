cmake_minimum_required(VERSION 3.16)

project(ProcessCounter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(includes)

if(WIN32)

    add_executable(main
        src/main_win.cpp
    )

    add_executable(worker
        src/worker_win.cpp
    )

else()
    add_executable(main
        src/main.cpp
    )

    add_executable(worker
        src/worker.cpp
    )

endif()

# Remove-Item -Recurse -Force build
# cmake -S . -B build -G "MinGW Makefiles"
# cmake --build build