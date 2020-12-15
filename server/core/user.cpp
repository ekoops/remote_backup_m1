#include "user.h"

std::string const &user::id() const { return this->id_; }

user &user::id(std::string const &id) {
    this->id_ = id;
    return *this;
}

std::string const &user::ip() const { return this->ip_; }

user &user::ip(std::string const &ip) {
    this->ip_ = ip;
    return *this;
}

std::string const &user::username() const {
    return this->username_;
}

user &user::username(std::string const &username) {
    this->username_ = username;
    return *this;
}

bool user::auth() const {
    return this->is_auth_;
}

user &user::auth(bool is_auth) {
    this->is_auth_ = is_auth;
    return *this;
}

bool user::synced() const {
    return this->is_synced_;
}

user &user::synced(bool is_synced) {
    this->is_synced_ = is_synced;
    return *this;
}

std::shared_ptr<directory::dir<directory::s_resource>> user::dir() {
    return this->dir_ptr_;
}

user &user::dir(boost::filesystem::path const &absolute_path) {
    this->dir_ptr_ = directory::dir<directory::s_resource>::get_instance(absolute_path);
    return *this;
}
bool user::operator==(user const &other) const {
    return this->id_ == other.id_;
}
