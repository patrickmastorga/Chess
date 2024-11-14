#include "uci_engine.h"

#include <chrono>
#include <thread>
#include <stdexcept>
#include <sstream>

UCIEngine::UCIEngine(std::string engine_path, std::string log_path) :
    engine_path(engine_path),
    log_path(log_path),
    child_process(engine_path),
    uci_active(false) {}

bool UCIEngine::start()
{
    // set up the log
    log = std::ofstream(log_path);
    return log.is_open() && child_process.create_child();
}

bool UCIEngine::uci_init()
{
    if (!is_running()) {
        return false;
    }

    // send "uci" command to engine
    if (!child_process.write_line("uci")) {
        return false;
    }
    log << "uci" << std::endl;

    // wait for response from engine
    std::string engine_output = "";
    uci_active = child_process.read_until(engine_output, "uciok", std::chrono::milliseconds(1000));
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
    if (!child_process.write_line(command)) {
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
    if (!child_process.write_line("isready")) {
        return false;
    };
    log << "isready" << std::endl;

    // wait for response from engine
    std::string engine_output = "";
    bool result = child_process.read_until(engine_output, "readyok", std::chrono::milliseconds(1000));
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
    if (!child_process.write_line(command)) {
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
    if (!child_process.write_line(command)) {
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
    child_process.write_line(command);
    log << command << std::endl;
    
    // wait for response from engine
    std::string engine_output = "";
    child_process.read_until(engine_output, "bestmove", think_time + std::chrono::milliseconds(1000));
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

void UCIEngine::close()
{
    if (!is_running()) {
        return;
    }

    if (!uci_active) {
        child_process.terminate();
        return;
    }

    // send "quit" command to engine
    child_process.write_line("quit");
    log << "quit" << std::endl;

    // wait for response from engine
    std::string engine_output = "";
    child_process.read_until(engine_output, "doesnt matter", std::chrono::milliseconds(1000));
    log << engine_output;

    child_process.terminate();
    return;
}

bool UCIEngine::is_running()
{
    return child_process.is_running();
}

UCIEngine::~UCIEngine()
{
    log.close();
    child_process.terminate();
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
