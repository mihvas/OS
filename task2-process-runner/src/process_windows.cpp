#include "process_runner.h"

#include <string>

static std::string quote_argument(const std::string& argument) {
    std::string result = "\"";
    int backslashes = 0;

    for (char c : argument) {
        if (c == '\\') {
            backslashes++;
        } else if (c == '"') {
            result.append(backslashes * 2 + 1, '\\');
            result += '"';
            backslashes = 0;
        } else {
            result.append(backslashes, '\\');
            result += c;
            backslashes = 0;
        }
    }

    result.append(backslashes * 2, '\\');
    result += '"';

    return result;
}

ProcessId start_process(
    const std::string& program,
    const std::vector<std::string>& arguments
) {
    std::string commandLine = quote_argument(program);

    for (const std::string& argument : arguments) {
        commandLine += " " + quote_argument(argument);
    }

    STARTUPINFOA startupInfo = {};
    PROCESS_INFORMATION processInfo = {};

    startupInfo.cb = sizeof(startupInfo);

    if (!CreateProcessA(
        program.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo
    )) {
        return nullptr;
    }

    CloseHandle(processInfo.hThread);

    return processInfo.hProcess;
}

int wait_process(ProcessId process) {
    if (process == nullptr) {
        return -1;
    }

    if (WaitForSingleObject(process, INFINITE) != WAIT_OBJECT_0) {
        CloseHandle(process);
        return -1;
    }

    DWORD exitCode;

    if (!GetExitCodeProcess(process, &exitCode)) {
        CloseHandle(process);
        return -1;
    }

    CloseHandle(process);

    return static_cast<int>(exitCode);
}

bool is_process_running(ProcessId process) {
    if (process == nullptr) {
        return false;
    }

    return WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
}

bool kill_process(ProcessId process) {
    if (process == nullptr) {
        return false;
    }

    return TerminateProcess(process, 1) != 0;
}