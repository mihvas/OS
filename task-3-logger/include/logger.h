#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"

inline void write_log(const std::string& message)
{
    int fd = open("process.log", O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (fd == -1)
    {
        return;
    }

    flock(fd, LOCK_EX);

    std::string line = "[" + get_current_time() + "] " + message + "\n";

    write(fd, line.c_str(), line.size());

    flock(fd, LOCK_UN);

    close(fd);
}

#endif