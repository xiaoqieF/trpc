#pragma once

#include <array>
#include <deque>
#include <memory>
#include <vector>

#include "asio.hpp"
#include "clog/clog.h"
#include "trpc/message.h"
#include "trpc/router.hpp"

namespace trpc {
class Connection : public std::enable_shared_from_this<Connection>, public asio::noncopyable {
public:
    Connection(asio::io_service* io_service, Router* router, size_t timeout_seconds)
        : socket_(*io_service),
          router_(router),
          timer_(*io_service),
          timeout_seconds_(timeout_seconds) {}

    ~Connection() { close(); }

    void start() { read_head(); }
    bool has_closed() const { return has_closed_; }
    asio::ip::tcp::socket& get_socket() { return socket_; }
    void set_conn_id(int64_t id) { conn_id_ = id; }

private:
    void read_head() {
        reset_timer(); // 每次请求设定超时时间，超时则销毁当前链接
        // 为保证回调执行时 connection 不会被销毁, 使用 shared_ptr 持有
        asio::async_read(socket_, asio::buffer(head_buffer_.begin(), head_buffer_.size()),
                         [this, self = this->shared_from_this()](asio::error_code ec, size_t len) {
                             this->read_head_handler(ec, len);
                         });
    }

    void read_head_handler(asio::error_code ec, size_t) {
        if (!socket_.is_open()) {
            CLOG_WARN("socket already closed");
            return;
        }
        // 对方关闭连接
        if (ec) {
            CLOG_WARN("asio error happened, {}: {}", ec.value(), ec.message());
            close();
            return;
        }
        RpcHeader* header = reinterpret_cast<RpcHeader*>(head_buffer_.begin());
        uint32_t body_len = header->body_len;
        function_id_ = header->function_id;
        request_id_ = header->request_id;
        if (body_len == 0) { // 可能是心跳消息包
            cancel_timer();
            read_head();
            return;
        }
        read_body(body_len);
    }

    void read_body(size_t len) {
        if (body_buffer_.size() < len) {
            body_buffer_.resize(len);
        }
        asio::async_read(socket_, asio::buffer(body_buffer_.data(), len),
                         [this, self = this->shared_from_this()](asio::error_code ec, size_t len) {
                             this->read_body_handler(ec, len);
                         });
    }

    void read_body_handler(asio::error_code ec, size_t len) {
        cancel_timer();
        if (!socket_.is_open()) {
            CLOG_WARN("socket already closed");
            return;
        }
        if (ec) {
            CLOG_WARN("{}: {}", ec.value(), ec.message());
            close();
            return;
        }
        read_head();
        std::string function_result =
            router_->route(function_id_, std::string_view(body_buffer_.data(), len));
        response_internal(std::move(function_result));
    }

    void response_internal(std::string function_result) {
        RpcMsg msg{};
        msg.header =
            RpcHeader{request_id_, static_cast<uint32_t>(function_result.size()), function_id_};
        msg.content = std::move(function_result);
        write_queue_.emplace_back(std::move(msg));
        if (write_queue_.size() > 1) {
            // 上次注册的 write 事件还未执行，不要重复注册
            return;
        }
        write();
    }

    void write() {
        auto& msg = write_queue_.front();
        std::array<asio::const_buffer, 2> write_buffers;
        write_buffers[0] = asio::buffer(&msg.header, sizeof(msg.header));
        write_buffers[1] = asio::buffer(msg.content.data(), msg.content.size());
        asio::async_write(socket_, write_buffers,
                          [this, self = this->shared_from_this()](asio::error_code ec, size_t len) {
                              this->on_written(ec, len);
                          });
    }

    void on_written(asio::error_code ec, size_t len) {
        if (ec) {
            CLOG_WARN("{}: {}", ec.value(), ec.message());
            close();
            return;
        }
        write_queue_.pop_front();
        if (!write_queue_.empty()) {
            write();
        }
    }

    void close() {
        if (has_closed_) {
            return;
        }
        CLOG_TRACE("close connection, id: {}", conn_id_);
        asio::error_code ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        has_closed_ = true;
    }

    void reset_timer() {
        if (timeout_seconds_ == 0) {
            return;
        }
        CLOG_TRACE("connection id: {}, reset timer.", conn_id_);
        timer_.expires_from_now(std::chrono::seconds(timeout_seconds_));
        timer_.async_wait([this, self = this->shared_from_this()](asio::error_code ec) {
            if (this->has_closed() || ec) {
                return;
            }
            CLOG_TRACE("connection id: {}, timer expired, close...", conn_id_);
            this->close();
        });
    }

    void cancel_timer() {
        if (timeout_seconds_ == 0) {
            return;
        }
        timer_.cancel();
    }

    std::array<char, RPC_HEAD_LEN> head_buffer_;
    std::vector<char> body_buffer_;
    uint64_t request_id_{0};
    uint32_t function_id_{0};

    bool has_closed_{false};
    int64_t conn_id_{0};

    asio::ip::tcp::socket socket_;
    Router* router_;
    std::deque<RpcMsg> write_queue_;

    asio::steady_timer timer_; // 超时关闭当前 connection
    size_t timeout_seconds_;
};
} // namespace trpc
