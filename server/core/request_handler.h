#ifndef REMOTE_BACKUP_M1_SERVER_REQUEST_HANDLER_H
#define REMOTE_BACKUP_M1_SERVER_REQUEST_HANDLER_H

#include <string>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include "../../shared/communication/message.h"
#include "../../shared/communication/tlv_view.h"
#include "user.h"
#include <condition_variable>
#include <unordered_set>
#include "../communication/message_queue.h"
#include "../utilities/logger.h"
#include "open_streams.h"


/*
 * This class allows to manage incoming client request
 * maintaining the ofstream opened during a communication
 * session to improve the efficiency.
 */
class request_handler : private boost::noncopyable {
    boost::filesystem::path backup_root_;
    boost::filesystem::path credentials_path_;
    open_streams streams_;

    void handle_auth(communication::tlv_view &msg_view,
                     communication::message_queue &replies,
                     user &user);

    void handle_list(communication::message_queue &replies,
                     user &user);

    void handle_create(communication::tlv_view &msg_view,
                       communication::message_queue &replies,
                       user &user);

    void handle_update(communication::tlv_view &msg_view,
                       communication::message_queue &replies,
                       user &user);

    void handle_erase(communication::tlv_view &msg_view,
                      communication::message_queue &replies,
                      user &user);

public:
    // Handle a request and produce a reply.
    explicit request_handler(
            boost::filesystem::path backup_root,
            boost::filesystem::path credentials_path
    );

    void handle_request(
            const communication::message &request,
            communication::message_queue &replies,
            user &usr
    );

    open_streams &streams();
};

#endif //REMOTE_BACKUP_M1_SERVER_REQUEST_HANDLER_H
