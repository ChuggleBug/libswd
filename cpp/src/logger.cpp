
#include <stdarg.h>
#include <stdio.h>

#include "libswd/logger.h"

// Static member intializations
LogLevel Logger::level = LogLevel::NONE;
bool Logger::logger_set = false;
const char *Logger::newline = "\n\r";

static Logger *logger;

// Mostly here to make it easier to see alignment
static const char *log_level_str[4] = {"[DEBUG]", "[INFO ]", "[WARN ]", "[ERROR]"};

void Logger::setLogger(Logger *l) {
    logger = l;
    Logger::logger_set = true;
}

bool Logger::isSet() { return Logger::logger_set; }

void Logger::setLogLevel(LogLevel level) { Logger::level = level; }

void Logger::setNewline(const char *str) { Logger::newline = str; }

void Logger::debug(const char *fmt, ...) {
    if (!writeAtLevel(LogLevel::DEBUG))
        return;
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    writeLine(log_level_str[0], buffer);
}

void Logger::info(const char *fmt, ...) {
    if (!writeAtLevel(LogLevel::INFO))
        return;
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    writeLine(log_level_str[1], buffer);
}

void Logger::warn(const char *fmt, ...) {
    if (!writeAtLevel(LogLevel::WARN))
        return;
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    writeLine(log_level_str[2], buffer);
}

void Logger::error(const char *fmt, ...) {
    if (!writeAtLevel(LogLevel::ERROR))
        return;
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    writeLine(log_level_str[3], buffer);
}

void Logger::writeLine(const char *level_str, const char *str) {
    logger->write(level_str);
    logger->write(" ");
    logger->write(str);
    logger->write(newline);
    logger->flush();
}

inline bool Logger::writeAtLevel(LogLevel level) {
    return Logger::logger_set && Logger::level <= level;
}
