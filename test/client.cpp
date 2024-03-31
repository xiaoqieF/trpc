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
        auto i = client.call<std::string>("get_dummy", 1, 2.0);
        clog::info("i: {}", i);
        auto ret = client.call<int, 1000>("hello", 1, 2);
        clog::info("ret: {}", ret);
        auto f = client.call<std::string, 1000>("get_fun_name", Fun{1, "xiaoqie", 2});
        clog::info("f: {}", f);
        auto fun = client.call<Fun>("get_fun");
        clog::info("id: {}, name: {}, age: {}", fun.id, fun.name, fun.age);
        auto res = client.call<double>("ff", 1, 2);
        clog::info("ret: {}", res);
        client.call<void>("print");
    } catch (const std::exception& e) {
        clog::error("{}", e.what());
    }
}