#include "uci_engine.h"

#include <sys/wait.h>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <sstream>

UCIEngine::UCIEngine(std::filesystem::path engine_path, std::filesystem::path log_file_path) : uci_active(false)
{
    // set up the log
    log = std::ofstream(log_file_path);
    if (!log.is_open()) {
        throw new std::runtime_error("Log file could not be opened!");
    }
    
    int parent_to_child[2];
    int child_to_parent[2];

    // create pipes between child and parent
    if (pipe(parent_to_child) == -1 || pipe(child_to_parent) == -1) {
        throw std::runtime_error("Failed to create pipes");
    }

    // fork process (create child)
    pid = fork();
    if (pid == -1) {
        throw std::runtime_error("Failed to fork process");
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
        execl(engine_path.c_str(), engine_path.c_str(), NULL);

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
}

bool UCIEngine::uci_init()
{
    if (!is_running()) {
        return false;
    }

    // send "uci" command to engine
    if (!write_line("uci")) {
        return false;
    }
    log << "uci" << std::endl;

    // wait for response from engine
    std::string engine_output = "";
    uci_active = read_until(engine_output, "uciok", std::chrono::milliseconds(1000));
    log << engine_output;

    // parse the output for engine name/id and options
    std::istringstream stream(engine_output);
    std::string line;
    while (std::getline(stream, line)) {
        std::vector<std::string> words = parse_words(line);
        if (words.empty()) {
            continue;
        }
        if (words[0] == "id") {
            std::string value = "";
            for (uint32 i = 2; i < words.size(); i++) {
                if (!value.empty()) {
                    value += ' ';
                }
                value += words[i];
            }
            if (words[1] == "name") {
                name = value;
            } else if (words[1] == "author") {
                author = value;
            }
        } else if (words[0] == "option") {
            options.emplace_back(words);
        }
    }
    return uci_active;
}

bool UCIEngine::set_debug(bool value)
{
    if (!is_running() || !uci_active) {
        return false;
    }

    // send "debug" command to engine
    std::string command = value ? "debug on" : "debug off";
    if (!write_line(command)) {
        return false;
    }
    log << command << std::endl;

    // small delay to prevent overloading engine with commands
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
}

bool UCIEngine::is_ready()
{
    if (!is_running() || !uci_active) {
        return false;
    }

    // send "uci" command to engine
    if (!write_line("isready")) {
        return false;
    };
    log << "isready" << std::endl;

    // wait for response from engine
    std::string engine_output = "";
    bool result = read_until(engine_output, "readyok", std::chrono::milliseconds(1000));
    log << engine_output;

    return result;
}

bool UCIEngine::set_option(std::string name, std::string value)
{
    if (!is_running() || !uci_active) {
        return false;
    }

    // send "debug" command to engine
    std::string command = "setoption name " + name + (value.empty() ? "" : " value " + value);
    if (!write_line(command)) {
        return false;
    }
    log << command << std::endl;

    // small delay to prevent overloading engine with commands
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
}

bool UCIEngine::set_position(std::string start, std::vector<std::string> &moves)
{
    if (!is_running() || !uci_active) {
        return false;
    }

    // create command
    std::string command;
    if (start == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" || start == "startpos") {
        command = "position startpos";
    
    } else {
        command = "position fen " + start;
    }
    if (!moves.empty()) {
        command += " moves";
        for (std::string move : moves) {
            command += ' ';
            command += move;
        }
    }

    // send position command to engine
    if (!write_line(command)) {
        return false;
    }
    log << command << std::endl;

    // small delay to prevent overloading engine with commands
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
}

bool UCIEngine::set_position(std::string start)
{
    std::vector<std::string> moves;
    return set_position(start, moves);
}

bool UCIEngine::set_position(Game &game)
{
    std::vector<std::string> moves;
    for (const Move &move : game.game_moves) {
        moves.push_back(move.as_long_algebraic());
    }
    return set_position(game.beginning_fen, moves);
}

bool UCIEngine::set_position(Board &board)
{
    return set_position(board.as_fen());
}

std::string UCIEngine::best_move(std::chrono::milliseconds think_time)
{
    if (!is_ready()) {
        return "";
    }

    // send "go" command to engine
    std::string command = "go movetime " + std::to_string(think_time.count());
    write_line(command);
    log << command << std::endl;
    
    // wait for response from engine
    std::string engine_output = "";
    read_until(engine_output, "bestmove", think_time + std::chrono::milliseconds(1000));
    log << engine_output;

    // parse the output for engine name/id and options
    std::vector<std::string> words = parse_words(engine_output);
    for (uint32 i = 0; i < words.size() - 1; i++) {
        if (words[i] == "bestmove") {
            return words[i + 1];
        }
    }

    return "";
}

void UCIEngine::quit()
{
    if (!is_running()) {
        return;
    }

    if (!uci_active) {
        terminate_child();
        return;
    }

    // send "quit" command to engine
    write_line("quit");
    log << "quit" << std::endl;

    // wait for response from engine
    std::string engine_output = "";
    read_until(engine_output, "doesnt matter", std::chrono::milliseconds(1000));
    log << engine_output;

    terminate_child();
    return;
}

bool UCIEngine::is_running()
{
    int status;
    // returns 0 if child process is still running
    return waitpid(pid, &status, WNOHANG) == 0;
}

bool UCIEngine::write_line(std::string line)
{
    line += '\n';
    return write(fd_write_to_engine, line.c_str(), line.size()) != -1;
}

void UCIEngine::terminate_child()
{
    if (is_running()) {
        // send termination signal to the child
        kill(pid, SIGTERM);
        // wait for child to terminate and clean up
        waitpid(pid, NULL, 0);
    }
}

UCIEngine::~UCIEngine()
{
    log.close();
    terminate_child();
}

bool UCIEngine::read_until(std::string &data_read, std::string match, std::chrono::milliseconds timeout)
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

std::vector<std::string> UCIEngine::parse_words(std::string &string)
{
    std::istringstream iss(string);
    std::string word;
    std::vector<std::string> tokens;

    while (iss >> word) {
        tokens.push_back(word);
    }

    return tokens;
}

UCIOption::UCIOption(std::vector<std::string> &definition)
{
    std::string temp;
    std::string *current = &temp;

    for (uint32 i = 1; i < definition.size(); i++) {
        if (definition[i] == "name") {
            current = &name;
        } else if (definition[i] == "type") {
            current = &type;
        } else if (definition[i] == "default") {
            current = &default_value;
        } else if (definition[i] == "min") {
            current = &min_value;
        } else if (definition[i] == "max") {
            current = &max_value;
        } else if (definition[i] == "var") {
            current = &predefined_value;
        
        } else {
            if (!current->empty()) {
                *current += ' ';
            }
            *current += definition[i];
        }
    }
}

std::string UCIOption::as_string()
{
    std::string out = "";
    out += "name ";
    out += name;
    out += " type ";
    out += type;
    out += " default ";
    out += default_value;
    out += " min ";
    out += min_value;
    out += " max ";
    out += max_value;
    out += " var ";
    out += predefined_value;
    return out;
}
