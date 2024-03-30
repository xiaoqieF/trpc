#pragma once
#include <string_view>

#include "trpc/codec.hpp"
#include "trpc/message.h"

namespace trpc {
class RpcResult {
public:
    RpcResult(std::string_view data)
        : content_(data.data(), data.size()) {}

    template <typename T>
    T as() {
        if (content_.empty()) {
            throw std::logic_error("empty result!");
        }
        check_result();
        msgpack::object_handle obj;
        auto res = msgpack_codec::unpack<std::tuple<int, T>>(obj, content_.data(), content_.size());
        return std::get<1>(res);
    }

    void check_result() {
        msgpack::object_handle obj;
        auto tp = msgpack_codec::unpack<std::tuple<int>>(obj, content_.data(), content_.size());
        if (std::get<0>(tp) != static_cast<int>(FuncResultCode::OK)) {
            throw std::logic_error(get_error_msg());
        }
    }

private:
    std::string get_error_msg() {
        msgpack::object_handle obj;
        auto tp = msgpack_codec::unpack<std::tuple<int, std::string>>(obj, content_.data(),
                                                                      content_.size());
        return std::get<1>(tp);
    }
    std::string content_;
};

} // namespace trpc
