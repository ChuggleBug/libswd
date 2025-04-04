
#include "libswd/logger.h"

// Static member intializations
LogLevel Logger::level = LogLevel::NONE;
bool Logger::logger_set = false;
const char *Logger::newline = "\n\r";

static Logger *logger;

// Mostly here to make it easier to see alignment
static const char *log_level_str[4] = {
    "[DEBUG]",
    "[INFO ]",
    "[WARN ]",
    "[ERROR]"
};

void Logger::setLogger(Logger *l) { 
    logger = l;
    Logger::logger_set = true;
}

bool Logger::isSet() {
    return Logger::logger_set;
}

void Logger::setLogLevel(LogLevel level) {
    Logger::level = level;
}

void Logger::setNewline(const char *str) {
    Logger::newline = str;
}


void Logger::debug(const char* str) {
    if (writeAtLevel(LogLevel::DEBUG)) {
        writeLine(log_level_str[0], str);
    }
}

void Logger::info(const char* str) {
    if (writeAtLevel(LogLevel::INFO)) {
        writeLine(log_level_str[1], str);
    }
}

void Logger::warn(const char* str) {
    if (writeAtLevel(LogLevel::WARN)) {
        writeLine(log_level_str[2], str);
    }
}

void Logger::error(const char* str) {
    if (writeAtLevel(LogLevel::ERROR)) {
        writeLine(log_level_str[3], str);
    }
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
