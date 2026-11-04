#include "process_runner.h"

#include <string>

#ifdef _WIN32

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

#else


#include <cerrno>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

ProcessId start_process(
    const std::string& program,
    const std::vector<std::string>& arguments
) {
    pid_t pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        std::vector<char*> argv;

        argv.push_back(const_cast<char*>(program.c_str()));

        for (const std::string& argument : arguments) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }

        argv.push_back(nullptr);

        execvp(program.c_str(), argv.data());

        _exit(127);
    }

    return pid;
}

int wait_process(ProcessId process) {
    if (process <= 0) {
        return -1;
    }

    int status;

    while (waitpid(process, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }

        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return -1;
}

bool is_process_running(ProcessId process) {
    if (process <= 0) {
        return false;
    }

    siginfo_t info = {};

    if (waitid(
        P_PID,
        static_cast<id_t>(process),
        &info,
        WEXITED | WNOHANG | WNOWAIT
    ) < 0) {
        return false;
    }

    return info.si_pid == 0;
}

bool kill_process(ProcessId process) {
    if (process <= 0) {
        return false;
    }

    return kill(process, SIGKILL) == 0;
}

#endif