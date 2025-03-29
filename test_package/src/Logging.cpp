/*
 * Copyright (c) 2025 WehrWolff
 *
 * This file is part of babel, which is BSL-1.0 licensed.
 * See http://opensource.org/licenses/bsl-1.0
 */

#include <string>
#include <stdexcept>
#include <iostream>
#include <span>

#include "Logging.hpp"

Logger::Logger() = default;
Logger::Logger(bool verbose) : verbose(verbose) {}

[[nodiscard]] std::string Logger::formatMsg(Type type, std::string message) {
    switch (type) {
        using enum Type;
        case SUCCESS:
            return BOLD + GREEN + message + RESET;
        case PROGRESS:
            return GREEN + message + RESET;
        case HIGHLIGHT:
            return BOLD + MAGENTA + message + RESET;
        case INFO:
            return message;
        case STATUS:
            return CYAN + message + RESET;
        case WARNING:
            return YELLOW + "WARN: " + message + RESET;
        case ERROR:
            return RED + message + RESET;
        default:
            return BLUE + message + RESET;
    }
}

void Logger::puts(Type type, const std::string& msg) const {
    using enum Type;
    if (type == VERBOSE && !verbose) {
        return;
    }
    
    std::string message = msg;
    if (msg.find("-- ") == 0) {
        std::cout << "-- ";
        message = msg.substr(3);
    }

    if (type == VERBOSE && verbose) {
        type = INFO;
        message = "[VERBOSE] " + message;
    }

    std::cout << Logger::formatMsg(type, message) << '\n';
}

void Logger::puts(Type type, const std::string& msg, const std::string& tag) const {
    using enum Type;
    if (type == VERBOSE && !verbose) {
        return;
    }
    
    std::string message = msg;
    if (type == VERBOSE && verbose) {
        type = INFO;
        message = "[VERBOSE] " + message;
    }

    std::cout << "[" << tag << "] ";
    std::cout << Logger::formatMsg(type, message) << '\n';
}

void Logger::applyVerboseSettings(std::span<char*> args) {
    for (int i = 1; i < std::size(args); ++i) {
        if (std::string(args[i]) == "--verbose" || std::string(args[i]) == "-v") {
            verbose = true;
            return;
        }
    }
    verbose = false;
}

const std::string Logger::RESET = "\033[0m";
const std::string Logger::BOLD = "\033[1m";
const std::string Logger::RED = "\033[31m";
const std::string Logger::GREEN = "\033[32m";
const std::string Logger::YELLOW = "\033[33m";
const std::string Logger::BLUE = "\033[34m";
const std::string Logger::MAGENTA = "\033[35m";
const std::string Logger::CYAN = "\033[36m";


Logger& LoggerSingleton::getLogger() {
    return instance;
}