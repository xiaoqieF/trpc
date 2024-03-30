#pragma once

#include <mutex>

#include "clog/sinks/null_mutex.h"

namespace clog {
namespace details {
// All console sinks shared singleton mutex
struct ConsoleMutex {
    using mutex_t = std::mutex;
    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }
};

struct ConsoleNullMutex {
    using mutex_t = NullMutex;
    static NullMutex& mutex() {
        static NullMutex m;
        return m;
    }
};

} // namespace details
} // namespace clog
