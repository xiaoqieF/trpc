#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "asio.hpp"
#include "trpc/codec.hpp"
#include "trpc/md5.hpp"
#include "trpc/meta_util.hpp"

namespace trpc {

class Router : public asio::noncopyable {
public:
    template <typename F>
    void register_handler(const std::string& name, F f) {
        uint32_t key = MD5::MD5Hash32(name.data());
        func_name_map_.emplace(key, name);
        func_map_[key] = [f](std::string_view str) -> std::string {
            using args_tuple = typename FunctionTraits<F>::bare_params_type;
            msgpack::object_handle handle; // 该对象保存了 unpack 的结果，销毁时会使结果销毁
            try {
                auto params = msgpack_codec::unpack<args_tuple>(handle, str.data(), str.size());
                return call(f, std::move(params));
            } catch (const std::exception& e) {
                return msgpack_codec::pack_args_to_str(FuncResultCode::FAIL, e.what());
            }
        };
    }

    template <typename F, typename Self>
    void register_handler(const std::string& name, F f, Self* self) {
        uint32_t key = MD5::MD5Hash32(name.data());
        func_name_map_.emplace(key, name);
        func_map_[key] = [f, self](std::string_view str) -> std::string {
            using args_tuple = typename FunctionTraits<F>::bare_params_type;
            msgpack::object_handle handle; // 该对象保存了 unpack 的结果，销毁时会使结果销毁
            try {
                auto params = msgpack_codec::unpack<args_tuple>(handle, str.data(), str.size());
                return call_member(f, self, std::move(params));
            } catch (const std::exception& e) {
                return msgpack_codec::pack_args_to_str(FuncResultCode::FAIL, e.what());
            }
        };
    }

    std::string route(uint32_t key, std::string_view args) {
        std::string result;
        auto it = func_map_.find(key);
        if (it == func_map_.end()) {
            result = msgpack_codec::pack_args_to_str(FuncResultCode::FAIL, "unknown function");
        } else {
            result = it->second(args);
        }
        if (result.size() > UINT32_MAX) {
            result = msgpack_codec::pack_args_to_str(FuncResultCode::FAIL, "result too long");
        }
        return result;
    }

private:
    using RpcFunc = std::function<std::string(std::string_view)>;
    using FuncMap = std::unordered_map<uint32_t, RpcFunc>;
    using FuncNameMap = std::unordered_map<uint32_t, std::string>;

    template <typename F, typename... Args>
    static std::string call(const F& f, std::tuple<Args...> tp) {
        return call_helper(f, std::make_index_sequence<sizeof...(Args)>{}, std::move(tp));
    }

    template <typename F, size_t... I, typename... Args>
    static std::string call_helper(const F& f,
                                   const std::index_sequence<I...>&,
                                   std::tuple<Args...> tp) {
        if constexpr (std::is_same_v<std::result_of_t<F(Args...)>, void>) {
            f(std::move(std::get<I>(tp))...);
            return msgpack_codec::pack_args_to_str(FuncResultCode::OK);
        } else {
            auto res = f(std::move(std::get<I>(tp))...);
            return msgpack_codec::pack_args_to_str(FuncResultCode::OK, res);
        }
    }

    template <typename F, typename Self, typename... Args>
    static std::string call_member(const F& f, Self* self, std::tuple<Args...> tp) {
        return call_member_helper(f, self, std::make_index_sequence<sizeof...(Args)>{},
                                  std::move(tp));
    }

    template <typename F, typename Self, size_t... I, typename... Args>
    static std::string call_member_helper(const F& f,
                                          Self* self,
                                          std::index_sequence<I...>,
                                          std::tuple<Args...> tp) {
        if constexpr (std::is_same_v<std::result_of_t<F(Self*, Args...)>, void>) {
            (self->*f)(std::move(std::get<I>(tp))...);
            return msgpack_codec::pack_args_to_str(FuncResultCode::OK);
        } else {
            auto res = (self->*f)(std::move(std::get<I>(tp))...);
            return msgpack_codec::pack_args_to_str(FuncResultCode::OK, res);
        }
    }

    FuncMap func_map_;
    FuncNameMap func_name_map_;
};
} // namespace trpc
