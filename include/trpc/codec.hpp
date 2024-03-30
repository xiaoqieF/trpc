#pragma once

#include <type_traits>

#include "msgpack.hpp"

namespace trpc {
namespace msgpack_codec {

using buffer_type = msgpack::sbuffer;
constexpr size_t init_size = 2 * 1024;

template <typename... Args>
buffer_type pack_args(Args&&... args) {
    buffer_type buffer(init_size);
    // or make_tuple ?
    msgpack::pack(buffer, std::forward_as_tuple(std::forward<Args>(args)...));
    return buffer;
}

template <typename Arg, typename... Args, typename = std::enable_if_t<std::is_enum_v<Arg>>>
std::string pack_args_to_str(Arg arg, Args&&... args) {
    buffer_type buffer(init_size);
    msgpack::pack(buffer,
                  std::forward_as_tuple(static_cast<int>(arg), std::forward<Args>(args)...));
    /// TODO: avoid copy
    return std::string(buffer.data(), buffer.size());
}

template <typename T>
buffer_type pack(T&& t) {
    buffer_type buffer;
    msgpack::pack(buffer, std::forward<T>(t));
    return buffer;
}

// obj_handle stores decoded objects
template <typename T>
T unpack(msgpack::object_handle& obj_handle, const char* data, size_t length) {
    try {
        msgpack::unpack(obj_handle, data, length);
        return obj_handle.get().as<T>();
    } catch (...) {
        throw std::invalid_argument("unpack failed: Args not match!");
    }
}

} // namespace msgpack_codec
} // namespace trpc