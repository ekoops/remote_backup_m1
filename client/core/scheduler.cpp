#include "../../shared/utilities/tools.h"
#include <boost/function.hpp>
#include "scheduler.h"
#include "../../shared/communication/tlv_view.h"

namespace fs = boost::filesystem;

/**
 * Construct a scheduler instance for a given watched directory
 * and a given connection instance.
 *
 * @param io_context io_context
 * @param dir_ptr the watched directory std::shared_ptr
 * @param connection_ptr the connection std::shared_ptr
 * @return a new constructed scheduler instance
 */
scheduler::scheduler(
        boost::asio::io_context &io,
        std::shared_ptr<directory::dir<directory::c_resource>> dir_ptr,
        std::shared_ptr<connection> connection_ptr
) : dir_ptr_{std::move(dir_ptr)},
    connection_ptr_{std::move(connection_ptr)},
    io_{io} {}

/**
 * Construct a scheduler instance std::shared_ptr for a given watched directory
 * and a given connection instance.
 *
 * @param io_context io_context
 * @param dir_ptr the watched directory std::shared_ptr
 * @param connection_ptr the connection std::shared_ptr
 * @return a new constructed scheduler instance std::shared_ptr
 */
std::shared_ptr<scheduler> scheduler::get_instance(
        boost::asio::io_context &io,
        std::shared_ptr<directory::dir<directory::c_resource>> dir_ptr,
        std::shared_ptr<connection> connection_ptr
) {
    return std::shared_ptr<scheduler>(new scheduler{
            io,
            std::move(dir_ptr),
            std::move(connection_ptr)
    });
}

/**
 * Allow to handle reconnection task sending the
 * stored user auth information is the user is already
 * authenticated.
 *
 * @return void
 */
void scheduler::reconnect() {
    this->connection_ptr_->connect();
    if (this->auth_data_.authenticated()) {
        if (!this->auth(this->auth_data_) && !this->login()) {
            std::exit(EXIT_FAILURE);
        }
        this->sync();
    }
}

/**
 * Allow to handle CREATE message server response
 *
 * @param relative_path the relative path of the file related to the CREATE message
 * @param sign the sign of the file related to the CREATE message
 * @param response an optional containing the eventual server response
 * @return void
 */
void scheduler::handle_create(
        fs::path const &relative_path,
        std::string const &sign,
        std::optional<communication::message> const &response
) {
    auto rsrc_opt = this->dir_ptr_->rsrc(relative_path);
    if (!rsrc_opt) std::exit(EXIT_FAILURE);
    directory::c_resource rsrc = rsrc_opt.value();

    if (!response) {
        std::cout << " \u2717 CREATE on " << relative_path.string() << " failed. I'll retry..." << std::endl;
        this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(false));
        return;
    }
    communication::message const &response_msg = response.value();
    communication::tlv_view s_view{response_msg};
    if (response_msg.msg_type() == communication::MSG_TYPE::CREATE &&
        s_view.next_tlv() &&
        s_view.tlv_type() == communication::TLV_TYPE::ITEM &&
        sign == std::string{s_view.cbegin(), s_view.cend()} &&
        s_view.next_tlv() &&
        (s_view.tlv_type() == communication::TLV_TYPE::OK ||
         std::stoi(std::string{s_view.cbegin(), s_view.cend()}) ==
         communication::ERR_TYPE::ERR_CREATE_ALREADY_EXIST)) {
        std::cout << " \u2713 CREATE on " << relative_path.string() << " done." << std::endl;
        this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(true).exist_on_server(true));
    } else {
        std::cout << " \u2717 CREATE on " << relative_path.string() << " failed. I'll retry..." << std::endl;
        this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(false));
    }
}

/**
 * Allow to handle UPDATE message server response
 *
 * @param relative_path the relative path of the file related to the UPDATE message
 * @param sign the sign of the file related to the UPDATE message
 * @param response an optional containing the eventual server response
 * @return void
 */
void scheduler::handle_update(
        fs::path const &relative_path,
        std::string const &sign,
        std::optional<communication::message> const &response
) {
    auto rsrc_opt = this->dir_ptr_->rsrc(relative_path);
    if (!rsrc_opt) std::exit(EXIT_FAILURE);
    directory::c_resource rsrc = rsrc_opt.value();
    if (!response) {
        std::cout << " \u2717 UPDATE on " << relative_path.string() << " failed. I'll retry..." << std::endl;
        this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(false));
        return;
    }
    communication::message const &response_msg = response.value();
    communication::tlv_view s_view{response_msg};
    auto result = response_msg.msg_type() == communication::MSG_TYPE::UPDATE &&
                  s_view.next_tlv() &&
                  s_view.tlv_type() == communication::TLV_TYPE::ITEM &&
                  sign == std::string{s_view.cbegin(), s_view.cend()} &&
                  s_view.next_tlv() &&
                  (s_view.tlv_type() == communication::TLV_TYPE::OK ||
                   std::stoi(std::string{s_view.cbegin(), s_view.cend()}) ==
                   communication::ERR_TYPE::ERR_UPDATE_ALREADY_UPDATED);
    if (result) {
        std::cout << " \u2713 UPDATE on " << relative_path.string() << " done." << std::endl;
        this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(true));
    } else {
        std::cout << " \u2717 UPDATE on " << relative_path.string() << " failed. I'll retry..." << std::endl;
        this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(false));
    }

}

/**
 * Allow to handle ERASE message server response
 *
 * @param relative_path the relative path of the file related to the ERASE message
 * @param sign the sign of the file related to the ERASE message
 * @param response an optional containing the eventual server response
 * @return void
 */
void scheduler::handle_erase(
        fs::path const &relative_path,
        std::string const &sign,
        std::optional<communication::message> const &response
) {
    auto rsrc_opt = this->dir_ptr_->rsrc(relative_path);
    if (!rsrc_opt) std::exit(EXIT_FAILURE);
    directory::c_resource rsrc = rsrc_opt.value();
    if (!response) {
        std::cout << " \u2717 ERASE on " << relative_path.string() << " failed. I'll retry..." << std::endl;
        this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(false));
        return;
    }
    communication::message const &response_msg = response.value();
    communication::tlv_view s_view{response_msg};
    if (response_msg.msg_type() == communication::MSG_TYPE::ERASE &&
        s_view.next_tlv() &&
        s_view.tlv_type() == communication::TLV_TYPE::ITEM &&
        sign == std::string{s_view.cbegin(), s_view.cend()} &&
        s_view.next_tlv() &&
        s_view.tlv_type() == communication::TLV_TYPE::OK) {
        this->dir_ptr_->erase(relative_path);
        std::cout << " \u2713 ERASE on " << relative_path.string() << " done." << std::endl;
    } else {
        this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(false));
        std::cout << " \u2717 ERASE on " << relative_path.string() << " failed. I'll retry..." << std::endl;
    }
}

/**
 * Allow to start the login procedure
 *
 * @return true if the login procedure has been successful, false otherwise
 */
bool scheduler::login() {
    std::string username, password;
    boost::regex username_regex{"[a-z][a-z\\d_\\.]{7, 15}"};
    boost::regex password_regex{"[a-zA-Z\\d\\-\\.@$!%*?&]{8, 16}"};
    int general_attempts = 3;
    int field_attempts;

    while (general_attempts) {
        field_attempts = 3;
        std::cout << "Insert your username:" << std::endl;
        while (!(std::cin >> username) || !boost::regex_match(username, username_regex)) {
            std::cin.clear();
            std::cout << "Failed to get username. Try again (attempts left "
                      << --field_attempts << ")." << std::endl;
            if (!field_attempts) return false;
        }
        field_attempts = 3;
        std::cout << "Insert your password:" << std::endl;
        while (!(std::cin >> password) || !boost::regex_match(password, password_regex)) {
            std::cin.clear();
            std::cout << "Failed to get password. Try again (attempts left "
                      << --field_attempts << ")." << std::endl;
            if (!field_attempts) return false;
        }
        auth_data usr{username, password};
        if (this->auth(usr)) {
            this->auth_data_ = usr;
            return true;
        } else {
            this->connection_ptr_->cancel_keepalive();
            std::cout << "Authentication failed (attempts left " << --general_attempts << ")." << std::endl;
        }
    }
    return false;
}

/**
 * Allow to try to authenticate a given user.
 *
 * @param username the client user username
 * @param password the client user password
 * @return true if the user has been successfully authenticated, false otherwise
 */
bool scheduler::auth(auth_data &usr) {
    std::string const &username = usr.username();
    std::string const &password = usr.password();
    communication::message auth_msg{communication::MSG_TYPE::AUTH};
    auth_msg.add_TLV(communication::TLV_TYPE::USRN, username.size(), username.c_str());
    auth_msg.add_TLV(communication::TLV_TYPE::PSWD, password.size(), password.c_str());
    auth_msg.add_TLV(communication::TLV_TYPE::END);

    auto response = this->connection_ptr_->sync_post(auth_msg);
    if (boost::indeterminate(response.first)) {
        std::cerr << "Connection has been lost during authentication" << std::endl;
        std::exit(EXIT_FAILURE);
    } else if (response.first == false) return false; // response not obtained
    else {  // response obtained
        auto response_msg = response.second.value();
        communication::tlv_view view{response_msg};
        if (view.next_tlv() && view.tlv_type() == communication::TLV_TYPE::OK) {
            usr.authenticated(true);
            return true;
        } else return false;
    }

}

bool scheduler::retrieve(std::string const &sign) {
    auto splitted_sign = tools::split_sign(sign);
    fs::path const &relative_path = splitted_sign.first;
    std::string const &digest = splitted_sign.second;
    std::ostringstream oss;
    oss << " \u25CC Scheduling RETRIEVE for " << relative_path.string() << "..." << std::endl;
    std::cout << oss.str();
    communication::message retrieve_request{communication::MSG_TYPE::RETRIEVE};
    retrieve_request.add_TLV(communication::TLV_TYPE::ITEM, sign.size(), sign.c_str());
    retrieve_request.add_TLV(communication::TLV_TYPE::END);
    auto response = this->connection_ptr_->sync_post(retrieve_request);
    if (boost::indeterminate(response.first) || response.first == false) {
        std::cout << " \u2717 RETRIEVE on " << relative_path.string() << " failed." << std::endl;
        return false;
    }
    auto response_msg = response.second.value();
    communication::tlv_view view{response_msg};
    if (response_msg.msg_type() != communication::MSG_TYPE::RETRIEVE) {
        std::cout << " \u2717 RETRIEVE on " << relative_path.string() << " failed." << std::endl;
        return false;
    }
    boost::system::error_code ec;
    fs::path absolute_path = this->dir_ptr_->path() / relative_path;
    try {
        fs::create_directories(absolute_path.parent_path());
        fs::ofstream ofs{absolute_path, std::ios_base::binary};
        if (!ofs) {
            std::cout << " \u2717 RETRIEVE on " << relative_path.string() << " failed." << std::endl;
            return false;
        }

        while (view.next_tlv() && view.tlv_type() == communication::TLV_TYPE::ITEM &&
               sign == std::string{view.cbegin(), view.cend()} &&
               view.next_tlv() && view.tlv_type() == communication::TLV_TYPE::CONTENT) {
            std::copy(view.cbegin(), view.cend(), std::ostreambuf_iterator<char>(ofs));
        }
        if (view.tlv_type() != communication::TLV_TYPE::END) {
            std::cout << view.tlv_type() << std::endl;
            std::cout << " \u2717 RETRIEVE on " << relative_path.string() << " failed." << std::endl;
            return false;
        }
        ofs.close();
        // Comparing server file digest with the sent digest
        std::string c_digest = tools::MD5_hash(absolute_path, relative_path);
        if (c_digest != digest) {
            remove(absolute_path, ec);  // if digests doesn't match, remove created file
            if (ec) std::exit(EXIT_FAILURE);
            std::cout << " \u2717 RETRIEVE on " << relative_path.string() << " failed." << std::endl;
            return false;
        }
        std::cout << " \u2713 RETRIEVE on " << relative_path.string() << " done." << std::endl;
        return true;
    }
    catch (fs::filesystem_error &ex) {
        remove(absolute_path, ec);  // if digests doesn't match, remove created file
        if (ec) std::exit(EXIT_FAILURE);
        std::cout << " \u2717 RETRIEVE on " << relative_path.string() << " failed." << std::endl;
        return false;
    }
}

/**
 * Allow to handle a RESTORE operation.
 *
 * @return void
 */
void scheduler::restore() {
    communication::message request_msg{communication::MSG_TYPE::LIST};
    request_msg.add_TLV(communication::TLV_TYPE::END);
    std::cout << " \u25CC Scheduling RESTORE..." << std::endl;

    auto response = this->connection_ptr_->sync_post(request_msg);
    if (boost::indeterminate(response.first) || response.first == false) {
        std::cout << " \u2717 Failed to obtain server file list." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    auto response_msg = response.second.value();
    communication::tlv_view s_view{response_msg};
    communication::MSG_TYPE s_msg_type = response_msg.msg_type();
    if (s_msg_type != communication::MSG_TYPE::LIST ||
        !s_view.next_tlv() ||
        s_view.tlv_type() == communication::TLV_TYPE::ERROR) {
        std::cerr << " \u2717 RESTORE failed." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    do {
        if (s_view.tlv_type() == communication::TLV_TYPE::ITEM) {
            this->retrieve(std::string{s_view.cbegin(), s_view.cend()});
        }
    } while (s_view.next_tlv());
    std::cout << " \u2713 RESTORE done." << std::endl;
}

/**
 * Allow to handle a SYNC operation.
 *
 * @return void
 */
void scheduler::sync() {
    communication::message request_msg{communication::MSG_TYPE::LIST};
    request_msg.add_TLV(communication::TLV_TYPE::END);
    std::cout << " \u25CC Scheduling SYNC..." << std::endl;

    auto response = this->connection_ptr_->sync_post(request_msg);
    if (boost::indeterminate(response.first)) {
        return this->reconnect();
    } else if (response.first == false) std::exit(EXIT_FAILURE); // response not obtained

    auto response_msg = response.second.value();

    communication::tlv_view s_view{response_msg};
    communication::MSG_TYPE s_msg_type = response_msg.msg_type();
    if (s_msg_type != communication::MSG_TYPE::LIST ||
        !s_view.next_tlv() ||
        s_view.tlv_type() == communication::TLV_TYPE::ERROR) {
        std::cerr << "Failed to sync server state" << std::endl;
        std::exit(-1);
    }

    auto s_dir_ptr = directory::dir<directory::c_resource>::get_instance("S_DIR");

    // Checking for server elements that should be deleted or updated
    do {
        if (s_view.tlv_type() == communication::TLV_TYPE::ITEM) {
            std::string s_sign{s_view.cbegin(), s_view.cend()};
            auto splitted_sign = tools::split_sign(s_sign);
            fs::path const &relative_path = splitted_sign.first;
            std::string s_digest = splitted_sign.second;
            s_dir_ptr->insert_or_assign(relative_path, directory::c_resource{
                    boost::indeterminate, // unused field for server dir
                    true,   // unused field for server dir
                    s_digest
            });
            if (!this->dir_ptr_->contains(relative_path)) {
                this->erase(relative_path, s_digest);
            } else {
                auto rsrc = this->dir_ptr_->rsrc(relative_path).value();
                std::string const &c_digest = rsrc.digest();
                if (rsrc.digest() != s_digest) this->update(relative_path, c_digest);
                else this->dir_ptr_->insert_or_assign(relative_path, rsrc.synced(true).exist_on_server(true));

            }
        }
    } while (s_view.next_tlv());

    // Checking for server elements that should be created
    this->dir_ptr_->for_each([this, &s_dir_ptr](std::pair<fs::path, directory::c_resource> const &pair) {
        if (!s_dir_ptr->contains(pair.first)) {
            this->create(pair.first, pair.second.digest());
        };
    });
    std::cout << " \u2713 SYNC done." << std::endl;
}

/**
 * Allow to schedule a CREATE operation through the associated connection
 *
 * @param relative_path the relative path of the file that has to be created
 * @param sign the sign of the file that has to be created
 *
 * @return void
 */
void scheduler::create(fs::path const &relative_path, std::string const &digest) {
    boost::asio::post(this->io_, [this, relative_path, digest]() {
        directory::c_resource rsrc = directory::c_resource{
                boost::indeterminate,
                false,
                digest
        };
        std::ostringstream oss;
        oss << " \u25CC Scheduling CREATE for " << relative_path.string() << "..." << std::endl;
        std::cout << oss.str();
        this->dir_ptr_->insert_or_assign(relative_path, rsrc);

        std::string sign = tools::create_sign(relative_path, digest);
        auto f_msg = communication::f_message::get_instance(
                communication::MSG_TYPE::CREATE,
                this->dir_ptr_->path() / relative_path,
                sign
        );

        this->connection_ptr_->async_post(
                f_msg,
                boost::asio::bind_executor(
                        this->io_,
                        boost::bind(
                                &scheduler::handle_create,
                                this,
                                relative_path,
                                sign,
                                boost::placeholders::_1
                        )
                )
        );
    });
}

/**
 * Allow to schedule a UPDATE operation through the associated connection
 *
 * @param relative_path the relative path of the file that has to be updated
 * @param sign the sign of the file that has to be updated
 *
 * @return void
 */
void scheduler::update(fs::path const &relative_path, std::string const &digest) {
    boost::asio::post(this->io_, [this, relative_path, digest]() {
        directory::c_resource rsrc = directory::c_resource{
                boost::indeterminate,
                true,
                digest
        };
        std::ostringstream oss;
        oss << " \u25CC Scheduling UPDATE for " << relative_path.string() << "..." << std::endl;
        std::cout << oss.str();
        this->dir_ptr_->insert_or_assign(relative_path, rsrc);

        std::string sign = tools::create_sign(relative_path, digest);
        auto f_msg = communication::f_message::get_instance(
                communication::MSG_TYPE::UPDATE,
                this->dir_ptr_->path() / relative_path,
                sign
        );

        this->connection_ptr_->async_post(
                f_msg,
                boost::asio::bind_executor(
                        this->io_,
                        boost::bind(
                                &scheduler::handle_update,
                                this,
                                relative_path,
                                sign,
                                boost::placeholders::_1
                        )
                )
        );
    });
}

/**
 * Allow to schedule a ERASE operation through the associated connection
 *
 * @param relative_path the relative path of the file that has to be erased
 * @param sign the sign of the file that has to be erased
 *
 * @return void
 */
void scheduler::erase(fs::path const &relative_path, std::string const &digest) {
    boost::asio::post(this->io_, [this, relative_path, digest]() {
        directory::c_resource rsrc = directory::c_resource{
                boost::indeterminate,
                true,
                digest
        };
        std::ostringstream oss;
        oss << " \u25CC Scheduling ERASE for " << relative_path.string() << "..." << std::endl;
        std::cout << oss.str();
        this->dir_ptr_->insert_or_assign(relative_path, rsrc);

        std::string sign = tools::create_sign(relative_path, digest);
        communication::message request_msg{communication::MSG_TYPE::ERASE};
        request_msg.add_TLV(communication::TLV_TYPE::ITEM, sign.size(), sign.c_str());
        request_msg.add_TLV(communication::TLV_TYPE::END);
        this->connection_ptr_->async_post(
                request_msg,
                boost::asio::bind_executor(
                        this->io_,
                        boost::bind(
                                &scheduler::handle_erase,
                                this,
                                relative_path,
                                sign,
                                boost::placeholders::_1
                        )
                )
        );
    });
}

