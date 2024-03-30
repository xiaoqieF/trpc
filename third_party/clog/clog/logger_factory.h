#pragma once

#include "clog/async_logger.h"
#include "clog/common.h"
#include "clog/logger.h"
#include "clog/sinks/std_color_sink.h"

namespace clog {

namespace sync_factory {
template <typename Sink, typename... SinkArgs>
std::unique_ptr<Logger> create(std::string logger_name, SinkArgs &&...args) {
    auto sinker = std::make_shared<Sink>(std::forward<SinkArgs>(args)...);
    return make_unique<Logger>(std::move(logger_name), sinker);
}
}; // namespace sync_factory

namespace async_factory {
template <typename Sink, typename... SinkArgs>
std::unique_ptr<Logger> create(std::string logger_name, SinkArgs &&...args) {
    auto sinker = std::make_shared<Sink>(std::forward<SinkArgs>(args)...);
    return make_unique<AsyncLogger>(std::move(logger_name), sinker);
}
} // namespace async_factory

class LoggerFactory {
public:
    static LoggerFactory &instance() {
        static LoggerFactory loggerFactory;
        return loggerFactory;
    }

    void setDefaultLogger(std::unique_ptr<Logger> logger) { default_logger_ = std::move(logger); }
    Logger *defaultLogger() { return default_logger_.get(); }

private:
    LoggerFactory() { default_logger_ = sync_factory::create<sinks::StdColorSinkNullMutex>(""); };
    std::unique_ptr<Logger> default_logger_;
};

} // namespace clog
