#pragma once

#include "clog/logger_factory.h"

namespace clog {
inline Logger *defaultLogger() { return LoggerFactory::instance().defaultLogger(); }

inline void setDefaultLogger(std::unique_ptr<Logger> logger) {
    LoggerFactory::instance().setDefaultLogger(std::move(logger));
}

inline void setLogLevel(LogLevel level) { defaultLogger()->setLevel(level); }

inline void setPattern(const std::string &pattern) { defaultLogger()->setPattern(pattern); }

template <typename... Args>
void trace(format_string_t<Args...> fmt, Args &&...args) {
    defaultLogger()->trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(format_string_t<Args...> fmt, Args &&...args) {
    defaultLogger()->debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void info(format_string_t<Args...> fmt, Args &&...args) {
    defaultLogger()->info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(format_string_t<Args...> fmt, Args &&...args) {
    defaultLogger()->warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void error(format_string_t<Args...> fmt, Args &&...args) {
    defaultLogger()->error(fmt, std::forward<Args>(args)...);
}

template <typename T>
void trace(const T &msg) {
    defaultLogger()->trace(msg);
}

template <typename T>
void debug(const T &msg) {
    defaultLogger()->debug(msg);
}

template <typename T>
void info(const T &msg) {
    defaultLogger()->info(msg);
}

template <typename T>
void warn(const T &msg) {
    defaultLogger()->warn(msg);
}

template <typename T>
void error(const T &msg) {
    defaultLogger()->error(msg);
}

#ifndef CLOG_NO_SOURCE_LOC
    #define CLOG_LOG(logger, level, ...) \
        (logger)->log(clog::SourceLocation{__FILE__, __LINE__, __FUNCTION__}, level, __VA_ARGS__)
#else
    #define CLOG_LOG(logger, level, ...) (logger)->log(clog::SourceLocation{}, level, __VA_ARGS__)
#endif

#define CLOG_TRACE(...) CLOG_LOG(clog::defaultLogger(), clog::LogLevel::TRACE, __VA_ARGS__)
#define CLOG_DEBUG(...) CLOG_LOG(clog::defaultLogger(), clog::LogLevel::DEBUG, __VA_ARGS__)
#define CLOG_INFO(...)  CLOG_LOG(clog::defaultLogger(), clog::LogLevel::INFO, __VA_ARGS__)
#define CLOG_WARN(...)  CLOG_LOG(clog::defaultLogger(), clog::LogLevel::WARN, __VA_ARGS__)
#define CLOG_ERROR(...) CLOG_LOG(clog::defaultLogger(), clog::LogLevel::ERROR, __VA_ARGS__)

} // namespace clog
