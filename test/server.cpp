#include <iostream>
#include <map>
#include <string>
#include <tuple>

#include "clog/clog.h"
#include "trpc/codec.hpp"
#include "trpc/connection.hpp"
#include "trpc/io_service_pool.hpp"
#include "trpc/message.h"
#include "trpc/meta_util.hpp"
#include "trpc/router.hpp"
#include "trpc/rpc_client.hpp"
#include "trpc/rpc_server.hpp"

struct Fun {
    void operator()(int, int) {}
    double ff(int a, double b) { return a + b; }

    int id;
    std::string name;
    int age;

    MSGPACK_DEFINE(id, name, age);
};

int hello(int a, int b) { return a + b; }
std::string get_dummy(int jj, double b) { return std::string{"hello"}; }

Fun get_fun() { return Fun{1, "xiaoqie", 20}; }

std::string get_fun_name(const Fun& f) { return f.name; }

int main() {
    trpc::RpcServer server(6666, 2, 500);
    server.register_handler("hello", hello);
    server.register_handler("get", get_dummy);
    Fun f;
    server.register_handler("ff", &Fun::ff, &f);
    server.register_handler("get_fun", get_fun);
    server.register_handler("get_fun_name", get_fun_name);
    server.run();
}