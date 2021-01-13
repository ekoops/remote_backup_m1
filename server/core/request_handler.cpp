#include "request_handler.h"
#include "../../shared/utilities/tools.h"
#include "../../shared/communication/f_message.h"
#include <boost/filesystem.hpp>
#include <utility>
#include <boost/algorithm/hex.hpp>

namespace fs = boost::filesystem;
namespace comm = communication;

/**
 * Construct a request_handler instance with a given backup folder
 * and a given user credentials file path.
 *
 * @param backup_root the backup root folder of all client backups
 * @param credentials_path the user credentials file path to authenticate them.
 * @return void
 */
request_handler::request_handler(
        fs::path backup_root,
        fs::path credentials_path
) : backup_root_{std::move(backup_root)},
    credentials_path_{std::move(credentials_path)} {}

/**
 * An helper to finalize the response. It adds
 * a specified TLV tag (OK or ERROR usually) and
 * in case of error it adds the error code to
 * the response.
 */
void close_response(
        comm::message_queue &replies,
        comm::TLV_TYPE tlv_type,
        comm::ERR_TYPE err_type = comm::ERR_TYPE::ERR_NONE
) {
    size_t length = 0;
    char const *buffer = nullptr;
    if (err_type != comm::ERR_TYPE::ERR_NONE) {
        auto err_type_str = std::to_string(err_type);
        length = err_type_str.size();
        buffer = err_type_str.c_str();
    }
    replies.add_TLV(tlv_type, length, buffer);
    replies.add_TLV(comm::TLV_TYPE::END);
}


/**
 * Handle authentication task given specific user data
 *
 * @param msg_view tlv_view of the request message containing user data
 * @param replies container for server responses
 * @param user the user session information
 * @return void
 */
void request_handler::handle_auth(
        comm::tlv_view &msg_view,
        comm::message_queue &replies,
        user &user
) {
    std::string username;
    std::string password;
    if (msg_view.tlv_type() != comm::TLV_TYPE::USRN)
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_AUTH_NO_USRN);

    username = std::string{msg_view.cbegin(), msg_view.cend()};

    if (!msg_view.next_tlv() || msg_view.tlv_type() != comm::TLV_TYPE::PSWD)
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_AUTH_NO_PSWD);


    password = std::string{msg_view.cbegin(), msg_view.cend()};

    if (tools::verify_password(this->credentials_path_, username, password)) {
        std::string user_id = tools::MD5_hash(username);
        user.id(user_id)
                .username(username)
                .dir(this->backup_root_.generic_path() / user_id)
                .auth(true);
        return close_response(replies, comm::TLV_TYPE::OK);
    } else return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_AUTH_FAILED);
}

/**
 * Handle sync task for a specific client
 *
 * @param replies container for server responses
 * @param user the client session information
 * @return void
 */
void request_handler::handle_list(
        comm::message_queue &replies,
        user &user
) {
    auto user_dir = user.dir();
    fs::path const &user_dir_path = user_dir->path();
    size_t user_dir_path_length = user_dir_path.size();

    try {
        for (auto &de : fs::recursive_directory_iterator(user_dir_path)) {
            fs::path const &absolute_path = de.path();
            if (fs::is_regular_file(absolute_path)) {
                fs::path relative_path{absolute_path.generic_path().string().substr(user_dir_path_length)};
                std::string digest = tools::MD5_hash(absolute_path, relative_path);
                if (!user_dir->insert_or_assign(relative_path, directory::s_resource{
                        true,
                        digest
                })) {
                    user_dir->clear();
                    return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_LIST_FAILED);
                }

                std::string sign = tools::create_sign(relative_path, digest);
                replies.add_TLV(comm::TLV_TYPE::ITEM, sign.size(), sign.c_str());
            }
        }
        user.synced(true);
        close_response(replies, comm::TLV_TYPE::OK);
    }
    catch (fs::filesystem_error &ex) {
        std::cout << "Filesystem error:\n\t" << ex.what() << std::endl;
        user_dir->clear();
        replies = comm::message_queue{comm::MSG_TYPE::LIST};
        close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_LIST_FAILED);
    }
}


/**
 * Handle create task for a specific file
 *
 * @param msg_view tlv_view of the request message containing file data
 * @param replies container for server responses
 * @param user the client session information
 * @return void
 */
void request_handler::handle_create(
        comm::tlv_view &msg_view,
        comm::message_queue &replies,
        user &user
) {
    // Check if request contains file metadata
    if (msg_view.tlv_type() != comm::TLV_TYPE::ITEM) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_CREATE_NO_ITEM);
    }
    std::string c_sign{msg_view.cbegin(), msg_view.cend()};
    auto splitted_c_sign = tools::split_sign(c_sign);
    fs::path &c_relative_path = splitted_c_sign.first;
    std::string c_digest = splitted_c_sign.second;
    auto user_dir = user.dir();

    replies.add_TLV(comm::TLV_TYPE::ITEM, c_sign.size(), c_sign.c_str());

    // Check if request contains file content
    if (!msg_view.next_tlv() || msg_view.tlv_type() != comm::TLV_TYPE::CONTENT) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_CREATE_NO_CONTENT);
    }

    auto rsrc = user_dir->rsrc(c_relative_path);
    // if the resource already exists on server and already synced
    if (rsrc && rsrc.value().synced()) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_CREATE_ALREADY_EXIST);
    }

    fs::path absolute_path{user_dir->path() / c_relative_path};
    boost::system::error_code ec;
    // creating necessary directories for containing the file that has to be created
    create_directories(absolute_path.parent_path(), ec);
    if (ec) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_CREATE_FAILED);
    }

    std::shared_ptr<fs::ofstream> ofs_ptr;
    bool is_last;
    try {
        auto result = this->streams_.get_stream(user, absolute_path);
        // file stream ptr
        ofs_ptr = result.first->second;
        if (!ofs_ptr || !*ofs_ptr) {
            return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_CREATE_FAILED);
        }

        // writing file data
        std::copy(msg_view.cbegin(), msg_view.cend(), std::ostreambuf_iterator<char>(*ofs_ptr));
        ofs_ptr->flush();

        bool is_first = result.second;
        is_last = msg_view.verify_end();

        // updating resource parameters on user dir view
        if (is_first) {
            user_dir->insert_or_assign(c_relative_path, directory::s_resource{
                    is_last, is_last ? c_digest : "TEMP"
            });
        } else if (is_last) {
            user_dir->insert_or_assign(c_relative_path, directory::s_resource{
                    true, c_digest
            });
        }
    } catch (fs::filesystem_error &ex) {
        std::cerr << "Filesystem error from " << ex.what() << std::endl;
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_CREATE_FAILED);
    }

    if (is_last) {
        ofs_ptr->close();
        this->streams_.erase_stream(user);
        std::string s_digest;
        try {
            // Comparing server file digest with the sent digest
            s_digest = tools::MD5_hash(absolute_path, c_relative_path);
            if (s_digest != c_digest) {
                remove(absolute_path, ec);  // if digests doesn't match, remove created file
                if (ec) std::exit(EXIT_FAILURE);
                user_dir->erase(c_relative_path);
                return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_CREATE_NO_MATCH);
            }
        }
        catch (fs::filesystem_error &ex) {
            std::cerr << "Failed to access to file:\n\t" << absolute_path << std::endl;
            remove(absolute_path, ec);
            if (ec) std::exit(EXIT_FAILURE);
            user_dir->erase(c_relative_path);
            return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_CREATE_FAILED);
        }
    }
    return close_response(replies, comm::TLV_TYPE::OK);
}

/**
 * Handle update task for a specific file
 *
 * @param msg_view tlv_view of the request message containing file data
 * @param replies container for server responses
 * @param user the client session information
 * @return void
 */
void request_handler::handle_update(
        comm::tlv_view &msg_view,
        comm::message_queue &replies,
        user &user
) {
    // Check if request contains file metadata
    if (msg_view.tlv_type() != comm::TLV_TYPE::ITEM) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_UPDATE_NO_ITEM);
    }
    std::string c_sign{msg_view.cbegin(), msg_view.cend()};
    auto splitted_c_sign = tools::split_sign(c_sign);
    fs::path &c_relative_path = splitted_c_sign.first;
    std::string c_digest = splitted_c_sign.second;
    auto user_dir = user.dir();

    replies.add_TLV(comm::TLV_TYPE::ITEM, c_sign.size(), c_sign.c_str());

    // Check if request contains file content
    if (!msg_view.next_tlv() || msg_view.tlv_type() != comm::TLV_TYPE::CONTENT) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_UPDATE_NO_CONTENT);
    }

    auto rsrc = user_dir->rsrc(c_relative_path);
    // if the resource doesn't exist on server
    if (!rsrc) return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_UPDATE_NOT_EXIST);
    // if the resource is already updated
    if (rsrc.value().digest() == c_digest) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_UPDATE_ALREADY_UPDATED);
    }

    fs::path absolute_path{user_dir->path() / c_relative_path};
    fs::path temp_path{absolute_path};
    temp_path += ".temp";

    std::shared_ptr<fs::ofstream> ofs_ptr;
    bool is_first;
    bool is_last;
    try {
        get_stream_result result = this->streams_.get_stream(user, temp_path);
        // file stream ptr
        ofs_ptr = result.first->second;
        if (!ofs_ptr || !*ofs_ptr) {
            return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_UPDATE_FAILED);
        }
        // writing file data
        std::copy(msg_view.cbegin(), msg_view.cend(), std::ostreambuf_iterator<char>(*ofs_ptr));

        is_first = result.second;
        is_last = msg_view.verify_end();

        // updating resource parameters on user dir view
        if (is_first) {
            user_dir->insert_or_assign(c_relative_path, directory::s_resource{
                    is_last, is_last ? c_digest : rsrc.value().digest()
            });
        } else if (is_last) {
            user_dir->insert_or_assign(c_relative_path, directory::s_resource{
                    true, c_digest
            });
        }
    } catch (fs::filesystem_error &ex) {
        std::cerr << "Filesystem error from " << ex.what() << std::endl;
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_UPDATE_FAILED);
    }

    if (is_last) {
        ofs_ptr->close();
        this->streams_.erase_stream(user);
        boost::system::error_code ec;
        remove(absolute_path, ec);
        if (ec) std::exit(EXIT_FAILURE);
        rename(temp_path, absolute_path, ec);
        if (ec) std::exit(EXIT_FAILURE);
        std::string s_digest;
        try {
            // Comparing server file digest with the sent digest
            s_digest = tools::MD5_hash(absolute_path, c_relative_path);
            if (s_digest != c_digest) {
                remove(absolute_path, ec);  // if digests doesn't match, remove created file
                if (ec) std::exit(EXIT_FAILURE);
                user_dir->erase(c_relative_path);
                return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_UPDATE_NO_MATCH);
            }
        }
        catch (fs::filesystem_error &ex) {
            std::cerr << "Failed to access to file:\n\t" << absolute_path << std::endl;
            remove(absolute_path, ec);
            if (ec) std::exit(EXIT_FAILURE);
            user_dir->erase(c_relative_path);
            return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_UPDATE_FAILED);
        }
    }
    return close_response(replies, comm::TLV_TYPE::OK);
}

/**
 * Handle erase task for a specific file
 *
 * @param msg_view tlv_view of the request message
 * @param replies container for server responses
 * @param user the client session information
 * @return void
 */
void request_handler::handle_erase(
        comm::tlv_view &msg_view,
        comm::message_queue &replies,
        user &user
) {
    // Check if request contains file metadata
    if (msg_view.tlv_type() != comm::TLV_TYPE::ITEM) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_ERASE_NO_ITEM);
    }
    std::string c_sign{msg_view.cbegin(), msg_view.cend()};
    auto splitted_c_sign = tools::split_sign(c_sign);
    fs::path &c_relative_path = splitted_c_sign.first;
    std::string c_digest = splitted_c_sign.second;
    auto user_dir = user.dir();

    replies.add_TLV(comm::TLV_TYPE::ITEM, c_sign.size(), c_sign.c_str());

    auto rsrc = user_dir->rsrc(c_relative_path);

    // if the resource doesn't exist on server or has a different digest
    if (!rsrc || rsrc.value().digest() != c_digest) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_ERASE_NO_MATCH);
    }

    fs::path absolute_path{user_dir->path() / c_relative_path};
    fs::path tmp{absolute_path};
    boost::system::error_code ec;
    // delete the file
    remove(tmp, ec);
    if (ec) {
        close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_ERASE_FAILED);
    }
    else {
        user_dir->erase(c_relative_path);
        close_response(replies, comm::TLV_TYPE::OK);
    }
    // deleting all empty directories that contained the deleted file
    tmp = tmp.parent_path();
    while (!ec && tmp != user_dir->path() && is_empty(tmp, ec)) {
        remove(tmp, ec);
        tmp = tmp.parent_path();
    }
}

void handle_retrieve(
        comm::tlv_view &msg_view,
        comm::message_queue &replies,
        user &user
) {
    if (msg_view.tlv_type() != comm::TLV_TYPE::ITEM) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_ERASE_NO_ITEM);
    }
    std::string c_sign{msg_view.cbegin(), msg_view.cend()};
    auto splitted_c_sign = tools::split_sign(c_sign);
    fs::path &c_relative_path = splitted_c_sign.first;
    std::string c_digest = splitted_c_sign.second;
    auto user_dir = user.dir();
    auto f_msg = communication::f_message::get_instance(
            communication::MSG_TYPE::RETRIEVE,
            user_dir->path() / c_relative_path,
            c_sign
    );
    try {
        replies = comm::message_queue {communication::MSG_TYPE::RETRIEVE};
        while (f_msg->next_chunk()) {
            replies.add_message(communication::message{
                    std::make_shared<std::vector<uint8_t>>(
                            f_msg->raw_msg_ptr()->cbegin(),
                            f_msg->raw_msg_ptr()->cend()
                    )
            });
        }
    }
    catch (std::exception& ex) {
        replies = comm::message_queue {communication::MSG_TYPE::RETRIEVE};
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_RETRIEVE_FAILED);
    }
}

/**
 * Handle all incoming request
 *
 * @param request the request that has to be served
 * @param replies container for server responses
 * @param user the client session information
 * @return void
 */
void request_handler::handle_request(
        const comm::message &request,
        comm::message_queue &replies,
        user &user
) {
    auto c_msg_type = request.msg_type();
    // assign the response message type to the reply.
    replies = comm::message_queue{c_msg_type};
    comm::tlv_view msg_view{request};
    if (!msg_view.next_tlv()) {
        return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_NO_CONTENT);
    }
    if (!user.auth()) {
        if (c_msg_type == comm::MSG_TYPE::AUTH) {
            return handle_auth(msg_view, replies, user);
        } else return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_MSG_TYPE_REJECTED);
    } else {
        if (!user.synced()) {
            if (c_msg_type == comm::MSG_TYPE::LIST) {
                return handle_list(replies, user);
            } else return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_MSG_TYPE_REJECTED);
        } else {
            if (c_msg_type == comm::MSG_TYPE::CREATE) {
                return handle_create(msg_view, replies, user);
            } else if (c_msg_type == comm::MSG_TYPE::UPDATE) {
                return handle_update(msg_view, replies, user);
            } else if (c_msg_type == comm::MSG_TYPE::ERASE) {
                return handle_erase(msg_view, replies, user);
            } else if (c_msg_type == comm::MSG_TYPE::RETRIEVE) {
                return handle_retrieve(msg_view, replies, user);
            } else if (c_msg_type == comm::MSG_TYPE::KEEP_ALIVE) {
                return close_response(replies, comm::TLV_TYPE::OK);
            } else return close_response(replies, comm::TLV_TYPE::ERROR, comm::ERR_TYPE::ERR_MSG_TYPE_REJECTED);
        }
    }
}

open_streams &request_handler::streams() {
    return this->streams_;
}