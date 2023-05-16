#include "core/util/Logger.h"
#include "core/application/Application.h"
#include "core/application/Engine.h"
#include <cstdarg>
#include <cassert>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <ctime>

int Logger::TIMESTAMP_FORMAT_SUBSECOND_DIGITS = std::chrono::hh_mm_ss<std::chrono::duration<long long, std::ratio<1, 10000000>>>::fractional_width;
int Logger::TIMESTAMP_FORMAT_LENGTH = 19 // yyyy-mm-dd hh:mm:ss
        + (TIMESTAMP_FORMAT_SUBSECOND_DIGITS > 0 ? 1 : 0) // decimal point before subsecond digits
        + TIMESTAMP_FORMAT_SUBSECOND_DIGITS; // subsecond digits

Logger::Logger() {
    m_buffer.resize(64);
}

Logger::~Logger() {

}

Logger* Logger::instance() {
    // This is a bit stinky... The logger doesn't own its own instance, however the application
    // should ultimately have control over the initialization of the main logger. This exists to
    // avoid having to include and use the application class to access the main logger
    return Application::instance()->logger();
}

const std::string& Logger::getOutputFilePath() const {
    return m_outputFilePath;
}

void Logger::log(Logger::LogLevel level, const char* format, ...) const {
    va_list args;
    va_start(args, format);
    logInternal(level, format, args);
    va_end(args);
}

void Logger::debug(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel_Debug, format, args);
    va_end(args);
}

void Logger::info(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel_Info, format, args);
    va_end(args);
}

void Logger::warn(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel_Warn, format, args);
    va_end(args);
}

void Logger::error(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel_Error, format, args);
    va_end(args);
}

void Logger::fatal(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    logInternal(LogLevel_Fatal, format, args);
    va_end(args);
}

void Logger::logInternal(Logger::LogLevel level, const char* format, va_list args) const {
    const char* levelPrefix = nullptr;
    const char* formatPrefix = nullptr;

    switch (level) {
        case LogLevel_Debug:
            levelPrefix = "DEBUG";
            formatPrefix = "\033[36m"; // cyan
            break;
        case LogLevel_Info:
            levelPrefix = "INFO";
            formatPrefix = "\033[0m"; // reset
            break;
        case LogLevel_Warn:
            levelPrefix = "WARNING";
            formatPrefix = "\033[33m"; // yellow
            break;
        case LogLevel_Error:
            levelPrefix = "ERROR";
            formatPrefix = "\033[31m"; // red
            break;
        case LogLevel_Fatal:
            levelPrefix = "FATAL";
            formatPrefix = "\033[31m"; // red
            break;
        default:
            assert(false);
    }

    const int minBufferSize = TIMESTAMP_FORMAT_LENGTH + 1 + 64 + 1;
    if (m_buffer.size() <= minBufferSize) {
        m_buffer.resize(minBufferSize);
    }

    int totalMessageLength = 0;


    int timestampOffset = 0;
    totalMessageLength += fast_format_timestamp(&m_buffer[timestampOffset]);
    m_buffer[totalMessageLength++] = '\0';

    int messageOffset = totalMessageLength;
    totalMessageLength += vsnprintf(&m_buffer[messageOffset], m_buffer.size() - messageOffset, format, args);
    if (totalMessageLength > m_buffer.size()) {
        m_buffer.resize(totalMessageLength + totalMessageLength / 2); // size grows by 50%
        vsnprintf(&m_buffer[messageOffset], m_buffer.size() - messageOffset, format, args);
    }
//    printf("[%s] [%s]: %s\n", &m_buffer[timestampOffset], levelPrefix, &m_buffer[messageOffset]);
    printf("%s[%s] [%s]: %s%s\n", formatPrefix != nullptr ? formatPrefix : "", &m_buffer[timestampOffset], levelPrefix, &m_buffer[messageOffset], formatPrefix != nullptr ? "\033[0m" : "");

    if (level >= LogLevel_Fatal) {
        try {
            Application::destroy();
        } catch (const std::exception& e) {
            printf("An error occurred while shutting down the application:\n%s\n", e.what());
            assert(false);
        } catch (const std::string& e) {
            printf("An error occurred while shutting down the application:\n%s\n", e.c_str());
            assert(false);
        } catch (...) {
            // Catch anything and everything
            printf("An unknown error occurred while shutting down the application\n");
            assert(false);
        }

        // This should be fine. The application is expected to have shut down by now, and if not, something went horribly wrong, so we have no choice.
        exit(-1);
    }
}



void fast_to_chars(char*, char* l, uint64_t x) {
    do {
        *(--l) = static_cast<char>('0' + (x % 10));
        x /= 10;
    } while(x != 0);
}


// Implementation copied from https://stackoverflow.com/questions/65646395/c-retrieving-current-date-and-time-fast
// This method should be very fast.
int Logger::fast_format_timestamp(char* outTimestampStr) {
    auto tp = std::chrono::system_clock::now();
    static auto const tz = std::chrono::current_zone();
    static auto info = tz->get_info(tp);
    if (tp >= info.end)
        info = tz->get_info(tp);
    auto tpl = std::chrono::local_days{} + (tp + info.offset - std::chrono::sys_days{});
    auto tpd = floor<std::chrono::days>(tpl);
    std::chrono::year_month_day ymd{tpd};
    std::chrono::hh_mm_ss hms{tpl-tpd};
    char* r = outTimestampStr;
    std::fill(r, r + TIMESTAMP_FORMAT_LENGTH - 1, '0');
    r[4] = r[7] = '-';
    r[10] = ' ';
    r[13] = r[16] = ':';
    fast_to_chars(r, r + 4, int{ymd.year()});
    fast_to_chars(r + 5,  r +7, unsigned{ymd.month()});
    fast_to_chars(r + 8, r + 10, unsigned{ymd.day()});
    fast_to_chars(r + 11, r + 13, hms.hours().count());
    fast_to_chars(r + 14, r + 16, hms.minutes().count());
    fast_to_chars(r + 17, r + 19, hms.seconds().count());
    if (TIMESTAMP_FORMAT_SUBSECOND_DIGITS > 0) {
        r[19] = '.';
        fast_to_chars(r + 20, r + 20 + TIMESTAMP_FORMAT_SUBSECOND_DIGITS, hms.subseconds().count());
    }

    return TIMESTAMP_FORMAT_LENGTH;
}
