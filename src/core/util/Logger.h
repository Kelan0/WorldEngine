#ifndef WORLDENGINE_LOGGER_H
#define WORLDENGINE_LOGGER_H

#include <string>
#include <iostream>
#include <vector>

#define LOG_DEBUG(...) Logger::instance()->debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::instance()->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::instance()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::instance()->error(__VA_ARGS__)
#define LOG_FATAL(...) Logger::instance()->fatal(__VA_ARGS__)

class Logger {
public:
    enum LogLevel {
        LogLevel_Debug = 0,
        LogLevel_Info = 1,
        LogLevel_Warn = 2,
        LogLevel_Error = 3,
        LogLevel_Fatal = 4,
    };

    static int TIMESTAMP_FORMAT_SUBSECOND_DIGITS;
    static int TIMESTAMP_FORMAT_LENGTH;
public:
    Logger();

    ~Logger();

    static Logger* instance();

    const std::string& getOutputFilePath() const;

    void log(LogLevel level, const char* format, ...) const;

    void debug(const char* format, ...) const;

    void info(const char* format, ...) const;

    void warn(const char* format, ...) const;

    void error(const char* format, ...) const;

    void fatal(const char* format, ...) const;

    // Outputs the current timestamp formatted as yyyy-mm-dd hh:mm:ss.fff
    static int fast_format_timestamp(char* outTimestampStr);

private:
    void logInternal(LogLevel level, const char* format, va_list args) const;

private:
    std::string m_outputFilePath;
    mutable std::vector<char> m_buffer;
};


#endif //WORLDENGINE_LOGGER_H
