/*
 * Copyright (c) 2025 WehrWolff
 *
 * This file is part of babel, which is BSL-1.0 licensed.
 * See http://opensource.org/licenses/bsl-1.0
 */

#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <string>
#include <stdexcept>
#include <iostream>
#include <span>

class Logger {
private:
    bool verbose;
    static const std::string RESET;
    static const std::string BOLD;
    static const std::string RED;
    static const std::string GREEN;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string MAGENTA;
    static const std::string CYAN;
public:
    Logger();
    explicit Logger(bool verbose);

    enum class Type {
        SUCCESS,
        PROGRESS,
        HIGHLIGHT,
        INFO,
        STATUS,
        VERBOSE,
        WARNING,
        ERROR
    };

    [[nodiscard]] static std::string formatMsg(Type type, std::string message);
    void puts(Type type, const std::string& msg) const;
    void puts(Type type, const std::string& msg, const std::string& tag) const;
    void applyVerboseSettings(std::span<char*> args);
};

class LoggerSingleton {
private:
    static Logger instance;
public:
    static Logger& getLogger();
};

#endif  /* LOGGING_HPP */