#ifndef REMOTE_BACKUP_M1_SERVER_USER_H
#define REMOTE_BACKUP_M1_SERVER_USER_H

#include "../../shared/directory/dir.h"
#include "../directory/s_resource.h"

/*
 * This class is used to
 */
class user {
    std::string id_;
    std::string username_;
    std::string ip_;
    bool is_auth_;
    bool is_synced_;
    std::shared_ptr<directory::dir<directory::s_resource>> dir_ptr_;
public:
    [[nodiscard]] std::string const &id() const;

    user &id(std::string const &id);

    [[nodiscard]] std::string const &ip() const;

    user &ip(std::string const &ip);

    [[nodiscard]] std::string const &username() const;

    user &username(std::string const &username);

    [[nodiscard]] bool auth() const;

    user &auth(bool is_auth);

    [[nodiscard]] bool synced() const;

    user &synced(bool is_synced);

    std::shared_ptr<directory::dir<directory::s_resource>> dir();

    user &dir(boost::filesystem::path const &absolute_path);
    bool operator==(user const &other) const;
};

namespace std {
    template <>
    struct hash<user> {
        std::size_t operator()(user const& user) const {
            return std::hash<std::string>()(user.id());
        }
    };
}

#endif //REMOTE_BACKUP_M1_SERVER_USER_H
