#include "clog/clog.h"
#include "trpc/rpc_client.hpp"

struct Fun {
    int id;
    std::string name;
    int age;

    MSGPACK_DEFINE(id, name, age);
};

int main() {
    trpc::RpcClient client("127.0.0.1", 6666);
    try {
        client.connect();
        auto i = client.call<1000, std::string>("get", 1, 2.0);
        clog::info("i: {}", i);
        auto ret = client.call<1000, int>("hello", 1, 2);
        clog::info("ret: {}", ret);
        auto f = client.call<1000, std::string>("get_fun_name", Fun{1, "xiaoqie", 2});
        clog::info("f: {}", f);
        auto fun = client.call<1000, Fun>("get_fun");
        clog::info("id: {}, name: {}, age: {}", fun.id, fun.name, fun.age);
    } catch (const std::exception& e) {
        clog::error("{}", e.what());
    }
}