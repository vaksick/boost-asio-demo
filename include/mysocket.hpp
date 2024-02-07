#pragma once
#include <boost/asio.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/thread.hpp>
#include <functional>
#include <memory>
#include <string>

namespace app {
    class session;

    using boost_socket_t = boost::asio::ip::tcp::socket;
    using boost_acceptor_t = boost::asio::ip::tcp::acceptor;
    using callback_t = std::function<void(std::shared_ptr<session>)>;

    class session : public std::enable_shared_from_this<session> {
        boost_socket_t handle;
        boost::asio::streambuf streambuf;
        size_t async_size_data(const boost::system::error_code &err, size_t bytes_count);
        void async_read(std::shared_ptr<session>, callback_t, const boost::system::error_code &err, size_t bytes_count);
        void async_write(std::shared_ptr<session>, const boost::system::error_code &err, size_t bytes_count);

    public:
        const int index;
        session(int index, boost::asio::io_context &context);
        ~session();
        boost_socket_t &socket();
        void _do(boost::asio::cancellation_signal &, callback_t);
        std::shared_ptr<session> getptr();

        void async_replay(const uint8_t *, size_t);
        void async_replay(const std::string &str) {
            async_replay(reinterpret_cast<const uint8_t *>(str.c_str()), str.size());
        }

        std::string to_string() const;
    };

    class service {
        boost::asio::io_context context;
        boost::thread_group threads;
        std::shared_ptr<boost_acceptor_t> acceptor;
        boost::asio::cancellation_signal cancel_signal;
        std::atomic<int> next_index;
        void handle_accept(std::shared_ptr<session>, callback_t, const boost::system::error_code &);
        service();

    public:
        ~service();
        //
        void bind(int, callback_t, int threads_count = 2);
        void bind_wait(int, callback_t, int threads_count = 2);
        void wait();
        void close();
        void close_wait();

        static service &instance();
    };
} // namespace app