#pragma once
#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <string>

#include "asio.hpp"
#include "trpc/md5.hpp"
#include "trpc/rpc_result.hpp"

namespace trpc {
class RpcClient : asio::noncopyable {
public:
    RpcClient(std::string host, unsigned short port)
        : host_(std::move(host)),
          port_(port),
          socket_(io_service_),
          work_(io_service_) {
        work_thread_ = std::thread([this]() { io_service_.run(); });
    }

    ~RpcClient() {
        std::promise<void> promise;
        // 为防止竞争，close()需要交给RpcClient本身的事件循环来做
        io_service_.post([this, &promise] {
            close();
            promise.set_value();
        });
        promise.get_future().wait();
        stop();
    }

    bool connect(std::string host, unsigned short port, size_t timeout = 3) {
        host_ = std::move(host);
        port_ = port;
        return connect(timeout);
    }

    bool connect(size_t timeout = 3) {
        if (has_connected_) {
            return true;
        }
        async_connect();
        return wait_connection(timeout);
    }

    bool wait_connection(size_t timeout) {
        if (has_connected_) {
            return true;
        }
        std::unique_lock lock(connection_mutex_);
        connection_cv_.wait_for(lock, std::chrono::seconds(timeout),
                                [this] { return has_connected_.load(); });
        return has_connected_;
    }

    void close() {
        if (!has_connected_) {
            return;
        }
        has_connected_ = false;
        asio::error_code ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

    void stop() {
        work_.~work();
        if (work_thread_.joinable()) {
            work_thread_.join();
        }
    }

    template <size_t TIMEOUT, typename T = void, typename... Args>
    std::enable_if_t<std::is_void_v<T>, T> call(const std::string& rpc_name, Args&&... args) {
        auto future_result = async_call(rpc_name, std::forward<Args>(args)...);
        auto status = future_result.wait_for(std::chrono::milliseconds(TIMEOUT));
        if (status == std::future_status::timeout || status == std::future_status::deferred) {
            CLOG_ERROR("future timeout or deferred");
            throw std::out_of_range("future timeout or deferred");
        }
        future_result.get().check_result();
    }

    template <size_t TIMEOUT, typename T = void, typename... Args>
    std::enable_if_t<!std::is_void_v<T>, T> call(const std::string& rpc_name, Args&&... args) {
        auto future_result = async_call(rpc_name, std::forward<Args>(args)...);
        auto status = future_result.wait_for(std::chrono::milliseconds(TIMEOUT));
        if (status == std::future_status::timeout || status == std::future_status::deferred) {
            CLOG_ERROR("future timeout or deferred");
            throw std::out_of_range("future timeout or deferred");
        }
        return future_result.get().template as<T>();
    }

    template <typename... Args>
    std::future<RpcResult> async_call(const std::string& rpc_name, Args&&... args) {
        std::promise<RpcResult> result;
        uint64_t req_id = request_id_;
        auto future = result.get_future();
        {
            std::lock_guard lock(result_map_mutex_);
            result_map_.emplace(request_id_++, std::move(result));
        }
        auto args_buffer = msgpack_codec::pack_args(std::forward<Args>(args)...);
        write(req_id, std::move(args_buffer), MD5::MD5Hash32(rpc_name.data()));
        return future;
    }

private:
    void async_connect() {
        auto addr = asio::ip::address::from_string(host_);
        socket_.async_connect({addr, port_}, [this](asio::error_code ec) {
            CLOG_TRACE("connected.");
            if (has_connected_) {
                return;
            }
            if (ec) {
                has_connected_ = false;
                CLOG_WARN("asio error happened, {}: {}", ec.value(), ec.message());
                return;
            }
            has_connected_ = true;
            do_read();
            connection_cv_.notify_one();
        });
    }

    void write(uint64_t request_id, msgpack_codec::buffer_type&& buffer, uint32_t func_id) {
        RpcMsg msg;
        size_t len = buffer.size();
        msg.header = {request_id, static_cast<uint32_t>(len), func_id};
        msg.content = std::string(buffer.data(), len);

        std::lock_guard lock(write_queue_mutex_);
        write_queue_.emplace_back(std::move(msg));
        if (write_queue_.size() > 1) {
            // 上次注册的 async_write 还未执行完，不要重复注册
            return;
        }

        do_write();
    }

    void do_write() {
        auto& msg = write_queue_.front();
        std::array<asio::const_buffer, 2> write_buffers;
        write_buffers[0] = asio::buffer(&msg.header, sizeof(msg.header));
        write_buffers[1] = asio::buffer(msg.content.data(), msg.content.size());
        asio::async_write(socket_, write_buffers, [this](asio::error_code ec, size_t len) {
            if (ec) {
                CLOG_WARN("asio error happened, {}: {}", ec.value(), ec.message());
                has_connected_ = false;
                close();
                return;
            }
            std::lock_guard lock(write_queue_mutex_);
            write_queue_.pop_front();
            if (!write_queue_.empty()) {
                do_write(); // 将队列全写出去
            }
        });
    }

    void do_read() {
        asio::async_read(socket_, asio::buffer(header_buffer_.data(), RPC_HEAD_LEN),
                         [this](asio::error_code ec, size_t len) { read_head_handler(ec, len); });
    }

    void read_head_handler(asio::error_code ec, size_t len) {
        if (!socket_.is_open()) {
            CLOG_WARN("socket already closed");
            has_connected_ = false;
            return;
        }
        if (ec) {
            CLOG_WARN("asio error happened, {}: {}", ec.value(), ec.message());
            close();
            return;
        }
        RpcHeader* header = reinterpret_cast<RpcHeader*>(header_buffer_.data());
        uint32_t body_len = header->body_len;
        if (body_len == 0) {
            CLOG_ERROR("Invalid body len");
            close();
            return;
        }
        read_body(header->request_id, body_len);
    }

    void read_body(uint64_t request_id, size_t body_len) {
        if (body_len > body_buffer_.size()) {
            body_buffer_.resize(body_len);
        }
        asio::async_read(socket_, asio::buffer(body_buffer_.data(), body_len),
                         [this, request_id](asio::error_code ec, size_t len) {
                             read_body_handler(request_id, ec, len);
                         });
    }

    void read_body_handler(uint64_t request_id, asio::error_code ec, size_t len) {
        if (!socket_.is_open()) {
            CLOG_WARN("socket already closed");
            handle_result(request_id, asio::error::make_error_code(asio::error::connection_aborted),
                          {});
            return;
        }
        if (ec) {
            CLOG_WARN("asio error happened, {}: {}", ec.value(), ec.message());
            has_connected_ = false;
            handle_result(request_id, ec, {});
            close();
        } else {
            handle_result(request_id, ec, {body_buffer_.data(), len});
            do_read();
        }
    }

    void handle_result(uint64_t request_id, asio::error_code ec, std::string_view data) {
        std::lock_guard lock(result_map_mutex_);
        if (result_map_.find(request_id) == result_map_.end()) {
            return;
        }
        auto& future = result_map_[request_id];
        if (ec) {
            future.set_value(RpcResult{""});
        } else {
            future.set_value(RpcResult{data});
        }
        result_map_.erase(request_id);
    }

    std::string host_;
    unsigned short port_{0};

    std::atomic_bool has_connected_{false};
    std::mutex connection_mutex_;
    std::condition_variable connection_cv_;

    asio::io_context io_service_;
    asio::ip::tcp::socket socket_;
    asio::io_context::work work_;
    std::thread work_thread_;

    std::array<char, RPC_HEAD_LEN> header_buffer_;
    std::vector<char> body_buffer_;
    std::unordered_map<uint64_t, std::promise<RpcResult>> result_map_;
    std::mutex result_map_mutex_;

    uint64_t request_id_{0};

    std::deque<RpcMsg> write_queue_;
    std::mutex write_queue_mutex_;
};
} // namespace trpc
