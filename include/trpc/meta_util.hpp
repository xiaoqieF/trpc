#pragma once

#include <functional>
#include <tuple>
#include <type_traits>

namespace trpc {
template <typename T>
struct FunctionTraits;

template <typename Ret, typename... Args>
struct FunctionTraits<Ret(Args...)> {
    using function_type = Ret(Args...);
    using return_type = Ret;
    using function_pointer = Ret (*)(Args...);

    using params_type = std::tuple<Args...>;
    using bare_params_type = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t args_num = sizeof...(Args);
};

template <typename Ret>
struct FunctionTraits<Ret()> {
    using function_type = Ret();
    using return_type = Ret;
    using function_pointer = Ret (*)();

    using tuple_type = std::tuple<>;
    using bare_params_type = std::tuple<>;
};

template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...)> : FunctionTraits<Ret(Args...)> {};

template <typename Ret, typename... Args>
struct FunctionTraits<std::function<Ret(Args...)>> : FunctionTraits<Ret(Args...)> {};

template <typename Ret, typename Class, typename... Args>
struct FunctionTraits<Ret (Class::*)(Args...)> : FunctionTraits<Ret(Args...)> {};

template <typename Ret, typename Class, typename... Args>
struct FunctionTraits<Ret (Class::*)(Args...) const> : FunctionTraits<Ret(Args...)> {};

// 这里为什么可以这样写 ?
template <typename Callable>
struct FunctionTraits : FunctionTraits<decltype(&Callable::operator())> {};

} // namespace trpc
