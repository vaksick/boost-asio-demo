#include "mysocket.hpp"
#include <boost/asio/read_until.hpp>
#include <spdlog/spdlog.h>

using namespace app;

session::session(int index, boost::asio::io_context &context) : handle(context), index(index) {
    spdlog::debug("new {}()", __FUNCTION__);
}

session::~session() {
    spdlog::debug("delete {}", __FUNCTION__);
}

boost_socket_t &session::socket() {
    return handle;
}

std::shared_ptr<session> session::getptr() {
    return shared_from_this();
}

using data_length_t = unsigned int;

size_t session::async_size_data(const boost::system::error_code &err, size_t bytes_count) {
    spdlog::debug("{}:{}: size = {}", __FUNCTION__, index, bytes_count);
    if (err) {
        spdlog::error("err[{}]: {}", index, err.message());
        return 0;
    } else if (bytes_count < sizeof(data_length_t)) {
        return sizeof(data_length_t) - bytes_count;
    } else if (bytes_count >= sizeof(data_length_t)) {
        union {
            char __array[sizeof(data_length_t)];
            data_length_t expected_size;
        };
        boost::asio::buffer_copy(boost::asio::buffer(__array), streambuf.data());
        spdlog::debug("streambuf.size() = {}", streambuf.size());
        if (expected_size > streambuf.size() - sizeof(data_length_t)) {
            return expected_size - (bytes_count - sizeof(data_length_t));
        }
    }
    return 0;
}

void session::async_read(std::shared_ptr<session> self, callback_t callback, const boost::system::error_code &err, size_t bytes_count) {
    spdlog::debug("{}:{}: size = {}", __FUNCTION__, index, bytes_count);
    if (err) {
        spdlog::error("err[{}]: {}", index, err.message());
    } else {
        callback(self);
    }
}

std::string session::to_string() const {
    if (streambuf.size() > sizeof(data_length_t)) {
        auto begin = boost::asio::buffers_begin(streambuf.data()) + sizeof(data_length_t);
        return std::string(begin, begin + (streambuf.size() - sizeof(data_length_t)));
    } else {
        return std::string();
    }
}

void session::_do(boost::asio::cancellation_signal &signal, callback_t callback) {
    using namespace boost::placeholders;
    spdlog::debug(__FUNCTION__);
    auto asyncSizeDataHandler = boost::asio::bind_cancellation_slot(signal.slot(), boost::bind(&session::async_size_data, this, _1, _2));
    auto asyncReadHandler = boost::asio::bind_cancellation_slot(signal.slot(), boost::bind(&session::async_read, this, shared_from_this(), callback, _1, _2));

    boost::asio::async_read(handle, streambuf, asyncSizeDataHandler, asyncReadHandler);
}

void session::async_write(std::shared_ptr<session>, const boost::system::error_code &err, size_t bytes_count) {
    spdlog::debug("{}:{}: size = {}", __FUNCTION__, index, bytes_count);
}

void session::async_replay(const uint8_t *data, size_t size) {
    using namespace boost::placeholders;
    spdlog::debug("{}:{}: size = {}", __FUNCTION__, index, size);
    union {
        char __array[sizeof(data_length_t)];
        data_length_t expected_size;
    };
    streambuf.consume(streambuf.size());
    expected_size = size;
    streambuf.commit(boost::asio::buffer_copy(streambuf.prepare(sizeof(data_length_t)), boost::asio::buffer(__array)));
    streambuf.commit(boost::asio::buffer_copy(streambuf.prepare(size), boost::asio::buffer(data, size)));
    boost::asio::async_write(handle,                                                              //
                             streambuf,                                                           //
                             boost::bind(&session::async_write, this, shared_from_this(), _1, _2) //
    );
}

service::service() {
    spdlog::debug(__FUNCTION__);
}

service::~service() {
    spdlog::debug(__FUNCTION__);
}

service &service::instance() {
    static service instance;
    return instance;
}

void service::bind_wait(int port, callback_t callback, int threads_count) {
    spdlog::debug(__FUNCTION__);
    bind(port, callback, threads_count);
    wait();
}

void service::wait() {
    spdlog::debug(__FUNCTION__);
    threads.join_all();
}

void service::close() {
    spdlog::debug(__FUNCTION__);
    cancel_signal.emit(boost::asio::cancellation_type::all);
    if (acceptor) {
        acceptor->close();
        acceptor.reset();
    }
}

void service::close_wait() {
    spdlog::debug(__FUNCTION__);
    close();
    wait();
}

void service::bind(int port, callback_t callback, int threads_count) {
    spdlog::debug(__FUNCTION__);
    close();
    threads_count = std::max(threads_count, 1);
    acceptor = std::make_shared<boost_acceptor_t>(context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));

    // auto acceptCompletionHandler = boost::asio::bind_cancellation_slot(cancel_signal.slot(), [this](const auto& e, auto s) {
    //         //OnAsioAsyncAcceptComplete(e, std::move(s));
    //         spdlog::debug("{}: bind_cancellation_slot", __FUNCTION__);
    //     });

    for (auto i = 0; i < threads_count; ++i) {
        using namespace boost::placeholders;
        int index = next_index++;
        auto handle = std::make_shared<::app::session>(index, context);
        spdlog::debug("{}:{}: acceptor->async_accept", __FUNCTION__, index);
        acceptor->async_accept(handle->socket(), boost::bind(&service::handle_accept, this, handle, callback, _1));
    }

    for (auto i = 0; i < threads_count; ++i)
        threads.create_thread(boost::bind(&boost::asio::io_context::run, &context));
}

void service::handle_accept(std::shared_ptr<session> handle, callback_t callback, const boost::system::error_code &err) {
    using namespace boost::placeholders;
    spdlog::debug("{}", __FUNCTION__);
    if (err) {
        spdlog::error("err[{}]: {}", handle->index, err.message());
        handle.reset();
    } else {
        try {
            handle->_do(cancel_signal, callback);
            //
            int index = next_index++;
            handle = std::make_shared<session>(index, context);
            spdlog::debug("{}:{}: acceptor->async_accept", __FUNCTION__, handle->index);
            acceptor->async_accept(handle->socket(), boost::bind(&service::handle_accept, this, handle, callback, _1));
        } catch (const std::exception &ex) {
            spdlog::error("{}: {}", __FUNCTION__, ex.what());
        }
    }
}