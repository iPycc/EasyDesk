#include "config/application_config.hpp"
#include "devices/device_repository.hpp"
#include "net/http_listener.hpp"
#include "storage/sqlite_database.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/signal_set.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

int main() {
    try {
        const auto config = easydesk::config::ApplicationConfig::fromEnvironment();
        const std::filesystem::path databasePath(config.databasePath);
        if (databasePath.has_parent_path()) {
            std::filesystem::create_directories(databasePath.parent_path());
        }

        easydesk::storage::SqliteDatabase database(config.databasePath);
        easydesk::devices::DeviceRepository devices(database);
        devices.migrate();

        boost::asio::io_context context(static_cast<int>(config.workerThreads));

        const auto address = boost::asio::ip::make_address(config.bindAddress);
        const boost::asio::ip::tcp::endpoint endpoint(address, config.port);
        std::make_shared<easydesk::net::HttpListener>(context, endpoint)->run();

        boost::asio::signal_set signals(context, SIGINT, SIGTERM);
        signals.async_wait([&context](const boost::system::error_code&, int) { context.stop(); });

        std::cout << "EasyDesk signaling listening on " << config.bindAddress << ':' << config.port
                  << " with " << config.workerThreads << " workers and " << devices.deviceCount()
                  << " registered devices\n";

        std::vector<std::thread> workers;
        workers.reserve(config.workerThreads - 1);
        for (std::size_t index = 1; index < config.workerThreads; ++index) {
            workers.emplace_back([&context] { context.run(); });
        }
        context.run();

        for (auto& worker : workers) {
            worker.join();
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
}
