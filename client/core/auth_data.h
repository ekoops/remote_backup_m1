#ifndef REMOTE_BACKUP_M1_CLIENT_USER_H
#define REMOTE_BACKUP_M1_CLIENT_USER_H
#include <string>

/*
 * This class is used to store the user
 * authentication data.
 */
class auth_data {
    std::string username_;
    std::string password_;
    bool authenticated_ = false;
public:
    auth_data() = default;
    auth_data(std::string username, std::string password);

    [[nodiscard]] std::string const& username() const;
    [[nodiscard]] std::string const& password() const;
    [[nodiscard]] bool authenticated() const;
    void authenticated(bool authenticated);
};


#endif //REMOTE_BACKUP_M1_CLIENT_USER_H
