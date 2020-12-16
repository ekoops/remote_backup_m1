#include <iostream>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include "core/file_watcher.h"
#include "core/connection.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;


po::variables_map parse_options(int argc, char const *const argv[]) {
    try {
        po::options_description desc("Backup client options");
        desc.add_options()
                ("help,h",
                 "produce help message")
                ("path-to-watch,P",
                 po::value<fs::path>()->default_value(
                         fs::path{"."}
                 ), "set path to watch")
                ("hostname,H",
                 po::value<std::string>()->required(),
                 "set backup server hostname")
                ("service,S",
                 po::value<std::string>()->required(),
                 "set backup server service name/port number")
                ("threads,T",
                 po::value<size_t>()->default_value(4),
                 "set worker thread pool size")
                ("delay,D",
                 po::value<size_t>()->default_value(5000),
                 "set file watcher refresh rate in milliseconds");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            std::exit(EXIT_SUCCESS);
        }

        po::notify(vm);

        // canonical returns the absolute path resolving symbolic link, dot and dot-dot
        // it also checks the existence
        vm.at("path-to-watch").value() = fs::canonical(vm["path-to-watch"].as<fs::path>());
        auto watched_dir = vm["path-to-watch"];
        auto watched_dir_path = watched_dir.as<fs::path>();
        if (!fs::is_directory(watched_dir_path)) {
            std::cerr << watched_dir_path.generic_path().string() << " is not a directory" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (watched_dir.defaulted()) {
            std::cout << "--path-to-watch option set to default value: "
                      << watched_dir_path.generic_path().string() << std::endl;
        }
        auto threads = vm["threads"];
        auto threads_val = threads.as<size_t>();
        if (threads.defaulted() || threads_val == 0 || threads_val > 16) {
            if (threads_val == 0) {
                vm.at("threads").value() = 1;
            } else if (threads_val > 16) {
                vm.at("threads").value() = 16;
            }
            std::cout << "--threads option set to default value: "
                      << vm["threads"].as<size_t>() << std::endl;
        }
        if (vm["delay"].defaulted()) {
            std::cout << "--delay option set to default value: "
                      << vm["delay"].as<size_t>() << std::endl;
        }
        return vm;
    }
    catch (std::exception &ex) {
        std::cout << "Error during options parsing:\n\t" << ex.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char const *const argv[]) {
    // Parsing program options
    po::variables_map vm = parse_options(argc, argv);

    try {
        // Destructuring program options
        fs::path path_to_watch = vm["path-to-watch"].as<fs::path>();
        std::string hostname = vm["hostname"].as<std::string>();
        std::string service = vm["service"].as<std::string>();
        size_t thread_pool_size = vm["threads"].as<size_t>();
        size_t delay = vm["delay"].as<size_t>();

        // Constructing an abstraction for the watched directory
        auto watched_dir_ptr = directory::dir<directory::c_resource>::get_instance(path_to_watch, true);
        boost::asio::io_context io_context;
        // Allowing generic SSL/TLS version
        boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23};
        ctx.load_verify_file("../files/certs/ca.pem");
        // Constructing an abstraction for handling SSL connection task
        auto connection_ptr = connection::get_instance(io_context, ctx);
        // Constructing an abstraction for scheduling async task and managing communication
        // with server through the connection
        auto scheduler_ptr = scheduler::get_instance(io_context, watched_dir_ptr, connection_ptr);
        // Constructing an abstraction for monitoring the filesystem and scheduling
        // server synchronizations through bind_scheduler
        file_watcher fw{watched_dir_ptr, scheduler_ptr, std::chrono::milliseconds{delay}};

        // prevent io_context object's run() calls from returning when there is no more work to do
        auto ex_work_guard_ = boost::asio::make_work_guard(io_context);
        std::vector<std::thread> thread_pool;
        thread_pool.reserve(thread_pool_size);
        for (int i = 0; i < thread_pool_size; i++) {
            thread_pool.emplace_back(
                    boost::bind(&boost::asio::io_context::run, &io_context),
                    std::ref(io_context)
            );
        }

        connection_ptr->set_reconnection_handler([scheduler_ptr]() {
            scheduler_ptr->reconnect();
        });

        // Performing server connection
        connection_ptr->resolve(hostname, service);
        connection_ptr->connect();
        // Starting login procedure
        if (!scheduler_ptr->login()) {
            std::cerr << "Authentication failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        // Starting specified directory local file watching
        fw.start();
        io_context.stop();

    }
    catch (fs::filesystem_error &e) {
        std::cerr << "Filesystem error from " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
    catch (boost::system::system_error &e) {
        std::cerr << "System error with code " << e.code() << ": " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
    catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::exit(EXIT_SUCCESS);
}