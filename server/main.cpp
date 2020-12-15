#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include "core/server.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

po::variables_map parse_options(int argc, char const *const argv[]) {
    try {
        std::string config_file{"../files/CONFIG.txt"};
        po::options_description desc("Backup server options");
        desc.add_options()
                ("help,h",
                 "produce help message")
                ("address,A",
                 po::value<std::string>()->required(),
                 "set backup server address")
                ("service,S",
                 po::value<std::string>()->required(),
                 "set backup server service name/port number")
                ("backup-root,R",
                 po::value<fs::path>()->default_value(
                         fs::path{"../files/backup_root"}
                 ), "set root backup directory")
                ("credentials-file,CF",
                 po::value<fs::path>()->default_value(
                         fs::path{"../files/USER.txt"}
                 ), "set the user credentials file path")
                ("logger-file,LF",
                 po::value<fs::path>()->default_value(
                         fs::path{"../files/LOG.txt"}
                 ), "set the logger file path")
                ("threads,T",
                 po::value<size_t>()->default_value(8),
                 "set worker thread pool size");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            std::exit(EXIT_SUCCESS);
        }

        fs::ifstream ifs{config_file.c_str()};
        if (!ifs) {
            std::cerr << " Failed to open " << config_file << " config file" << std::endl;
            std::exit(EXIT_FAILURE);
        } else po::store(parse_config_file(ifs, desc), vm);

        po::notify(vm);

        // canonical returns the absolute path resolving symbolic link, dot and dot-dot
        // it also checks the existence
        vm.at("backup-root").value() = fs::canonical(vm["backup-root"].as<fs::path>());
        auto backup_root = vm["backup-root"];
        auto backup_root_path = backup_root.as<fs::path>();
        if (!fs::is_directory(backup_root_path)) {
            std::cerr << backup_root_path.generic_path().string() << " is not a directory" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (backup_root.defaulted()) {
            std::cout << "--backup-root option set to default value: "
                      << backup_root_path.generic_path().string() << std::endl;
        }

        vm.at("credentials-file").value() = fs::canonical(vm["credentials-file"].as<fs::path>());
        auto credentials_file = vm["credentials-file"];
        auto credentials_file_path = credentials_file.as<fs::path>();
        if (!fs::is_regular_file(credentials_file_path)) {
            std::cerr << credentials_file_path.generic_path().string() << " is not a file" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (credentials_file.defaulted()) {
            std::cout << "--credentials-file option set to default value: "
                      << credentials_file_path.generic_path().string() << std::endl;
        }

        vm.at("logger-file").value() = fs::canonical(vm["logger-file"].as<fs::path>());
        auto log_file = vm["logger-file"];
        auto log_file_path = log_file.as<fs::path>();
        if (!fs::is_regular_file(log_file_path)) {
            std::cerr << log_file_path.generic_path().string() << " is not a file" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (credentials_file.defaulted()) {
            std::cout << "--logger-file option set to default value: "
                      << log_file_path.generic_path().string() << std::endl;
        }

        if (vm["threads"].defaulted()) {
            std::cout << "--threads option set to default value: "
                      << vm["threads"].as<size_t>() << std::endl;
        }

        return vm;
    }
    catch (std::exception &ex) {
        std::cout << "Error during options parsing:\n\t" << ex.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    try {
        server s{parse_options(argc, argv)};
        // Run the server until stopped.
        s.run();
    }
    catch (std::exception &ex) {
        std::cerr << "Exception:\n\t" << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}