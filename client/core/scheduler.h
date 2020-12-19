#ifndef REMOTE_BACKUP_M1_CLIENT_SCHEDULER_H
#define REMOTE_BACKUP_M1_CLIENT_SCHEDULER_H

#include <boost/filesystem.hpp>
#include "connection.h"
#include "../../shared/directory/dir.h"
#include "../directory/c_resource.h"
#include "auth_data.h"

/*
 * The scheduler instance will use the connection abstraction
 * to communicate with the server, and a thread pool for constructing
 * in parallel multiple request message. It also manages the user information
 * for reconnection.
 */

class scheduler {
    std::shared_ptr<connection> connection_ptr_;
    std::shared_ptr<directory::dir<directory::c_resource>> dir_ptr_;
    boost::asio::io_context &io_;
    // user authentication data
    auth_data auth_data_;

    scheduler(
            boost::asio::io_context &io,
            std::shared_ptr<directory::dir<directory::c_resource>> dir_ptr,
            std::shared_ptr<connection> connection_ptr
    );

    void handle_create(
            boost::filesystem::path const &relative_path,
            std::string const &sign,
            std::optional<communication::message> const &response
    );

    void handle_update(
            boost::filesystem::path const &relative_path,
            std::string const &sign,
            std::optional<communication::message> const &response
    );

    void handle_erase(
            boost::filesystem::path const &relative_path,
            std::string const &sign,
            std::optional<communication::message> const &response
    );

public:
    static std::shared_ptr<scheduler> get_instance(
            boost::asio::io_context &io,
            std::shared_ptr<directory::dir<directory::c_resource>> dir_ptr,
            std::shared_ptr<connection> connection_ptr
    );

    void reconnect();

    bool login();

    bool auth(auth_data &usr);

    bool retrieve(std::string const& sign);

    void restore();

    void sync();

    void create(boost::filesystem::path const &relative_path, std::string const &digest);

    void update(boost::filesystem::path const &relative_path, std::string const &digest);

    void erase(boost::filesystem::path const &relative_path, std::string const &digest);

};


#endif //REMOTE_BACKUP_M1_CLIENT_SCHEDULER_H
