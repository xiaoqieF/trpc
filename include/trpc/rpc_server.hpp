#pragma once

#include <atomic>
#include <condition_variable>
#include <thread>
#include <unordered_map>

#include "asio.hpp"
#include "trpc/io_service_pool.hpp"
#include "trpc/router.hpp"

namespace trpc {
using asio::ip::tcp;
class RpcServer : asio::noncopyable {
public:
    RpcServer(unsigned short port,
              size_t pool_size,
              size_t timeout_seconds = 15,
              size_t check_seconds = 10)
        : io_service_pool_(pool_size),
          acceptor_(io_service_pool_.next_io_service(), tcp::endpoint(tcp::v4(), port)),
          timeout_seconds_(timeout_seconds),
          check_seconds_(check_seconds),
          signals_(io_service_pool_.next_io_service()) {
        do_accept();
        clean_thread_ = std::thread([this]() { do_clean(); });
        running = true;
        CLOG_INFO("RPC Server running with {} threads", pool_size);

        signals_.add(SIGINT);
        signals_.add(SIGTERM);
        signals_.add(SIGQUIT);
        signals_.async_wait([this](std::error_code, int sig) {
            CLOG_INFO("receive signal: {}; stop server!", sig);
            stop();
        });
    }

    ~RpcServer() { stop(); }

    template <typename F>
    void register_handler(const std::string& name, F&& f) {
        router_.register_handler(name, std::forward<F>(f));
    }

    template <typename F, typename Self>
    void register_handler(const std::string& name, F&& f, Self* self) {
        router_.register_handler(name, std::forward<F>(f), self);
    }

    void run() { io_service_pool_.run(); }
    void stop() {
        if (!running) {
            return;
        }
        {
            std::unique_lock lock(conn_mutex_);
            stop_check_ = true;
            clean_cv_.notify_all();
        }
        clean_thread_.join();

        io_service_pool_.stop();
        running = false;
    }

private:
    void do_accept() {
        conn_.reset(
            new Connection(&(io_service_pool_.next_io_service()), &router_, timeout_seconds_));
        acceptor_.async_accept(conn_->get_socket(), [this](asio::error_code ec) {
            CLOG_TRACE("one client come.");
            if (!acceptor_.is_open()) {
                return;
            }
            if (ec) {
                CLOG_WARN("{}: {}", ec.value(), ec.message());
            } else {
                conn_->set_conn_id(conn_id_);
                conn_->start();
                std::lock_guard lock(conn_mutex_);
                connections_.emplace(conn_id_++, conn_);
                CLOG_TRACE("establish connection, id:{}.", conn_id_ - 1);
            }
            do_accept();
        });
    }

    void do_clean() {
        while (!stop_check_) {
            std::unique_lock lock(conn_mutex_);
            clean_cv_.wait_for(lock, std::chrono::seconds(check_seconds_));
            CLOG_TRACE("do_clean: search for connection...");

            for (auto it = connections_.begin(); it != connections_.end();) {
                if (it->second->has_closed()) {
                    CLOG_TRACE("clean connection, id:{}", it->first);
                    it = connections_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    IoServicePool io_service_pool_;
    asio::ip::tcp::acceptor acceptor_;
    size_t timeout_seconds_;
    Router router_;
    std::atomic<bool> running{false};

    std::shared_ptr<Connection> conn_;
    std::unordered_map<int64_t, std::shared_ptr<Connection>> connections_;
    int64_t conn_id_{0};
    std::mutex conn_mutex_;

    std::thread clean_thread_;
    std::condition_variable clean_cv_;
    size_t check_seconds_;
    std::atomic<bool> stop_check_{false};

    asio::signal_set signals_;
};
} // namespace trpc
