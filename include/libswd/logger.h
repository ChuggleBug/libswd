
#ifndef __LOGGER_H
#define __LOGGER_H

enum class LogLevel {
    DEBUG = 10,
    INFO = 20,
    WARN = 30,
    ERROR = 40,
    NONE = 100 // Needs to be the highest value
};

class Logger {

  public: 

    static void setLogger(Logger *logger);

    static bool isSet();

    static void setLogLevel(LogLevel level);

    static void setNewline(const char *str);

    static void debug(const char* fmt, ...);
    static void info(const char* fmt, ...);
    static void warn(const char* fmt, ...);
    static void error(const char* fmt, ...);

  private:

    static LogLevel level; // = LogLevel::None
    static bool logger_set; // = false
    static const char *newline; // = \n\r

    static void writeLine(const char *level_str, const char *str);
    static bool writeAtLevel(LogLevel level);

  protected:

    // Does not write the newline character
    virtual void write(const char *str) = 0;

    // Some implementations might benefit from a flush
    virtual void flush() {}
};

#endif // __LOGGER_H