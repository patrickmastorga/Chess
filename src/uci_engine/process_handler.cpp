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
        fd_write_to_child = parent_to_child[1];
        fd_read_from_child = child_to_parent[0];
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
    close(fd_read_from_child);
    close(fd_write_to_child);
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
        FD_SET(fd_read_from_child, &read_fds);

        // wait for data to become available
        int result = select(fd_read_from_child + 1, &read_fds, NULL, NULL, &timeout);
        if (result <= 0) {
            // timout occurred or error
            return false;
        }

        // read available data
        ssize_t bytes_read = read(fd_read_from_child, buffer, sizeof(buffer) - 1);
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
    return write(fd_write_to_child, line.c_str(), line.size()) != -1;
}
#endif
#ifdef _WIN32
#include <windows.h>

bool ProcessHandler::create_child() {
    HANDLE parent_to_child_read, parent_to_child_write;
    HANDLE child_to_parent_read, child_to_parent_write;
    
    // Security attributes for pipe inheritance
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    // Create pipes between child and parent
    if (!CreatePipe(&parent_to_child_read, &parent_to_child_write, &sa, 0) || 
        !CreatePipe(&child_to_parent_read, &child_to_parent_write, &sa, 0)) {
        return false;
    }

    // Set up the STARTUPINFOW structure for Unicode
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    // Redirect child process stdin and stdout
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = parent_to_child_read;
    si.hStdOutput = child_to_parent_write;
    si.hStdError = child_to_parent_write;  // If you also want to capture stderr

    // Command to execute
    std::wstring command = std::wstring(path.begin(), path.end());

    // Create the child process
    if (!CreateProcessW(
            NULL,                     // No module name (use command line)
            const_cast<wchar_t*>(command.c_str()), // Command line
            NULL,                     // Process handle not inheritable
            NULL,                     // Thread handle not inheritable
            TRUE,                     // Set handle inheritance to TRUE
            0,                        // No creation flags
            NULL,                     // Use parent's environment block
            NULL,                     // Use parent's starting directory 
            &si,                      // Pointer to STARTUPINFO structure
            &pi)) {                   // Pointer to PROCESS_INFORMATION structure
        return false;
    }

    // In the parent process, close unnecessary handles
    CloseHandle(parent_to_child_read);
    CloseHandle(child_to_parent_write);

    // Save necessary handles for communication
    h_write_to_child = parent_to_child_write;
    h_read_from_child = child_to_parent_read;

    // Also save process information if needed to manage the child process
    child_process_info = pi;

    return true;
}

bool ProcessHandler::is_running() {
    DWORD exitCode;
    // Check if the process is still running
    if (GetExitCodeProcess(child_process_info.hProcess, &exitCode)) {
        // If the exit code is STILL_ACTIVE, the process is running
        return exitCode == STILL_ACTIVE;
    }
    return false;
}

void ProcessHandler::terminate() {
    // Close the communication handles
    CloseHandle(h_read_from_child);
    CloseHandle(h_write_to_child);

    if (is_running()) {
        // Terminate the child process
        TerminateProcess(child_process_info.hProcess, 1);
        
        // Wait for child to terminate and clean up resources
        WaitForSingleObject(child_process_info.hProcess, INFINITE);

        // Close process and thread handles
        CloseHandle(child_process_info.hProcess);
        CloseHandle(child_process_info.hThread);
    }
}

bool ProcessHandler::read_until(std::string &data_read, std::string match, std::chrono::milliseconds timeout) {
    char buffer[256];

    // Record the start time
    auto timeout_time = std::chrono::steady_clock::now() + timeout;

    while (is_running()) {
        // Calculate remaining time for timeout
        auto now = std::chrono::steady_clock::now();
        DWORD remaining_time_ms = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout_time - now).count());

        if (remaining_time_ms <= 0) {
            return false;
        }

        // Wait for the handle to become ready for reading
        DWORD wait_result = WaitForSingleObject(h_read_from_child, remaining_time_ms);
        if (wait_result == WAIT_TIMEOUT) {
            // Timeout occurred
            return false;
        } else if (wait_result != WAIT_OBJECT_0) {
            // An error occurred
            return false;
        }

        // If we reached here, data should be available to read
        DWORD bytes_read = 0;
        BOOL read_result = ReadFile(h_read_from_child, buffer, sizeof(buffer) - 1, &bytes_read, NULL);

        if (!read_result || bytes_read == 0) {
            // No more bytes to read or error occurred
            return false;
        }

        // Null-terminate the buffer
        buffer[bytes_read] = '\0';
        data_read += buffer;

        // Check if the match is found
        size_t match_found = data_read.find(match);
        // Ensure that the entire line after the match string is read
        if (match_found != std::string::npos && data_read.substr(match_found).find('\n') != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool ProcessHandler::write_line(std::string line) {
    line += '\n';
    DWORD bytesWritten;
    return WriteFile(h_write_to_child, line.c_str(), line.size(), &bytesWritten, NULL) != 0;
}
#endif

