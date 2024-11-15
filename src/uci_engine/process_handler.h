#pragma once

#include <filesystem>
#include <string>

#ifdef __unix__
    #include <unistd.h>
#endif
#ifdef _WIN32
    #include <windows.h>
#endif

class ProcessHandler {
public:
    ProcessHandler(std::string path);
    ~ProcessHandler();

    // method to create a child process and set up pipes
    // returns true if process is successful
    bool create_child();

    // returns true if the child process is currenty running
    bool is_running();
    
    // method to close the pipes and terminate the child process
    void terminate();

    // reads input from the child process until either the match string is reached or a timeout occurs
    bool read_until(std::string &data_read, std::string match, std::chrono::milliseconds timeout);
    
    // writes the input to the child process (adds newline charecter)
    // returns true if successful
    bool write_line(std::string line);

private:
    std::string path;

#ifdef __unix__
    // file descriptor for write end of parent to child pipe
    int fd_write_to_child;
    // file descriptor for read end of child to parent pipe
    int fd_read_from_child;
    // process id for child process
    pid_t pid;
#endif
#ifdef _WIN32
    // handle object for write end of parent to child pipe
    HANDLE h_write_to_child;
    // handle object for read end of child to parent pipe
    HANDLE h_read_from_child;
    // process information about child process
    PROCESS_INFORMATION child_process_info;
#endif
};