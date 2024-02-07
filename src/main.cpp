#include <spdlog/spdlog.h>
#include <boost/asio/io_context.hpp>
#include "mysocket.hpp"

void sigal_processing(int sig) {
    spdlog::info("stoping ...");
    app::service::instance().close();
}

int main() {
    spdlog::info("start ...");
    spdlog::set_level(spdlog::level::debug);
    std::signal(SIGINT, sigal_processing);
    std::signal(SIGTERM, sigal_processing);
    try {
        app::service::instance().bind_wait(2001, [](std::shared_ptr<app::session> session){
            spdlog::debug(__FUNCTION__);
            session->async_replay("test");
        });
    } catch (const std::exception &ex) {
        spdlog::error("main: {}", ex.what());
        return EXIT_FAILURE;
    }
    spdlog::info("... stop");
    return EXIT_SUCCESS;
}