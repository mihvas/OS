#include "process_runner.h"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

ProcessId start_process(
    const std::string& program,
    const std::vector<std::string>& arguments
) {
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        return -1;
    }

    int flags = fcntl(pipefd[1], F_GETFD);

    if (flags == -1 || fcntl(pipefd[1], F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);

        std::vector<char*> argv;

        argv.push_back(const_cast<char*>(program.c_str()));

        for (const std::string& argument : arguments) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }

        argv.push_back(nullptr);
        if (execvp(program.c_str(), argv.data()) == -1) {
            int errorCode = errno;
            write(pipefd[1], &errorCode, sizeof(errorCode));

            _exit(127);
        }
    }

    close(pipefd[1]);

    int errorCode;
    ssize_t n = read(pipefd[0], &errorCode, sizeof(errorCode));

    close(pipefd[0]);

    if (n == -1) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        return -1;
    }

    if (n > 0) {
        waitpid(pid, nullptr, 0);
        errno = errorCode;
        return -1;
    }

    return pid;
            
}


int wait_process(ProcessId process) {
    if (process <= 0) {
        return -1;
    }

    int status;

    while (waitpid(process, &status, 0) == -1) {
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
    ) == -1) {
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