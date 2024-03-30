#pragma once

#include <memory>
#include <vector>

#include "asio.hpp"
#include "asio/detail/noncopyable.hpp"
#include "asio/steady_timer.hpp"
#include "clog/clog.h"

namespace trpc {
class IoServicePool : public asio::noncopyable {
public:
    explicit IoServicePool(size_t pool_size)
        : io_service_index_(0) {
        if (pool_size == 0) {
            CLOG_ERROR("IoServicePool size should > 0");
            exit(-1);
        }
        for (size_t i = 0; i < pool_size; ++i) {
            auto io_service = std::make_shared<asio::io_context>();
            auto io_work = std::make_shared<asio::io_context::work>(*io_service);
            io_services_.push_back(io_service);
            io_works_.push_back(io_work);
        }
    }

    void run() {
        // one io_service per thread
        std::vector<std::thread> threads;
        for (size_t i = 0; i < io_services_.size(); ++i) {
            threads.emplace_back(
                std::thread([](io_service_ptr service) { service->run(); }, io_services_[i]));
        }
        CLOG_INFO("Run io_service pool of {} threads", threads.size());
        for (auto& t : threads) {
            t.join();
        }
    }

    void stop() {
        for (auto service : io_services_) {
            service->stop();
        }
    }

    asio::io_context& next_io_service() {
        asio::io_context& io_service = *io_services_[io_service_index_++];
        io_service_index_ %= io_services_.size();
        return io_service;
    }

private:
    using io_service_ptr = std::shared_ptr<asio::io_context>;
    using io_work_ptr = std::shared_ptr<asio::io_context::work>;

    std::vector<io_service_ptr> io_services_;
    std::vector<io_work_ptr> io_works_;
    size_t io_service_index_{0};
};
} // namespace trpc