#ifndef PROCESS_RUNNER_H
#define PROCESS_RUNNER_H

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
using ProcessId = HANDLE;
#else
#include <sys/types.h>
using ProcessId = pid_t;
#endif

ProcessId start_process(
    const std::string& program,
    const std::vector<std::string>& arguments = {}
);

int wait_process(ProcessId process);

bool is_process_running(ProcessId process);

bool kill_process(ProcessId process);

#endif