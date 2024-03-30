#pragma once

namespace clog {
namespace details {
struct NullMutex {
    void lock() const {}
    void unlock() const {}
};
} // namespace details

} // namespace clog
