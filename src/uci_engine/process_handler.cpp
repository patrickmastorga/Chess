#include "process_handler.h"

ProcessHandler::ProcessHandler(std::string path) : path(path) {}

ProcessHandler::~ProcessHandler()
{
    terminate();
}

#ifdef __unix__
#include <sys/wait.h>

bool ProcessHandler::create_child()
{
    int parent_to_child[2];
    int child_to_parent[2];

    // create pipes between child and parent
    if (pipe(parent_to_child) == -1 || pipe(child_to_parent) == -1) {
        return false;
    }

    // fork process (create child)
    pid = fork();
    if (pid == -1) {
        return false;
    }

    if (pid == 0) {
        // in the child process
        // redirect stin and stout
        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        // no longer need the pipes (because we redirected stin and stout)
        close(parent_to_child[0]);
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        close(child_to_parent[1]);

        // execute chess engine in this process
        execl(path.c_str(), path.c_str(), NULL);

        // terminate program if this fails
        perror("execl failed");
        exit(1);
    } else {
        // in the parent process
        // close unused read/write ends of pipe
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        // save neccesary read/write ends
        fd_write_to_engine = parent_to_child[1];
        fd_read_from_engine = child_to_parent[0];
    }
    return true;
}

bool ProcessHandler::is_running()
{
    int status;
    // returns 0 if child process is still running
    return waitpid(pid, &status, WNOHANG) == 0;
}

void ProcessHandler::terminate()
{
    close(fd_read_from_engine);
    close(fd_write_to_engine);
    if (is_running()) {
        // send termination signal to the child
        kill(pid, SIGTERM);
        // wait for child to terminate and clean up
        waitpid(pid, NULL, 0);
    }
}

bool ProcessHandler::read_until(std::string &data_read, std::string match, std::chrono::milliseconds timeout)
{
    char buffer[256];

    // Record the start time
    auto timeout_time = std::chrono::steady_clock::now() + timeout;

    while (is_running()) {
        // Calculate the elapsed time
        auto now = std::chrono::steady_clock::now();
        int remaining_time_us = std::chrono::duration_cast<std::chrono::microseconds>(timeout_time - now).count();

        if (remaining_time_us <= 0) {
            return false;
        }

        // set up remaining time for select() in seconds and microseconds
        struct timeval timeout;
        timeout.tv_sec = remaining_time_us / 1'000'000;
        timeout.tv_usec = remaining_time_us % 1'000'000;

        // set up file descriptor set
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd_read_from_engine, &read_fds);

        // wait for data to become available
        int result = select(fd_read_from_engine + 1, &read_fds, NULL, NULL, &timeout);
        if (result <= 0) {
            // timout occurred or error
            return false;
        }

        // read available data
        ssize_t bytes_read = read(fd_read_from_engine, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            // no more bytes to be read or error
            return false;
        }

        buffer[bytes_read] = '\0';
        data_read += buffer;

        // check if the match is found
        size_t match_found = data_read.find(match);
        // ensure that the entire line after the match string is read
        if (match_found != std::string::npos && data_read.substr(match_found).find('\n') != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool ProcessHandler::write_line(std::string line)
{
    line += '\n';
    return write(fd_write_to_engine, line.c_str(), line.size()) != -1;
}
#endif
#ifdef _WIN32
    
#endif

