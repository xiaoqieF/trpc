#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace trpc {
enum class FuncResultCode : std::int16_t {
    OK = 0,
    FAIL = 1,
};

struct RpcHeader {
    uint64_t request_id;
    uint32_t body_len;
    uint32_t function_id;
};

struct RpcMsg {
    RpcHeader header;
    std::string content;
};

static constexpr size_t RPC_HEAD_LEN = sizeof(RpcHeader);
} // namespace trpc
