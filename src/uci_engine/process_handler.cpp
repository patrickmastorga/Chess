#include <process_handler.h>

ProcessHandler::ProcessHandler(std::string path) : path(path) {}

ProcessHandler::~ProcessHandler()
{
    terminate();
}

bool ProcessHandler::create_child()
{
    return false;
}

bool ProcessHandler::is_running()
{
    return false;
}

void ProcessHandler::terminate()
{
}

bool ProcessHandler::read_until(std::string &data_read, std::string match, std::chrono::milliseconds timeout)
{
    return false;
}

bool ProcessHandler::write_line(const std::string input)
{
    return false;
}
