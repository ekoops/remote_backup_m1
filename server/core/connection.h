#ifndef REMOTE_BACKUP_M1_SERVER_CONNECTION_H
#define REMOTE_BACKUP_M1_SERVER_CONNECTION_H


#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <vector>
#include <boost/array.hpp>
#include "../../shared/directory/dir.h"
#include "../../shared/communication/message.h"
#include "request_handler.h"
#include "user.h"
#include "../communication/message_queue.h"
#include "../utilities/logger.h"

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

/// Represents a single connection from a client.
class connection : public boost::enable_shared_from_this<connection>, private boost::noncopyable {

    /// Strand to ensure the connection's handlers are not called concurrently.
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    /// Socket for the connection.
    ssl_socket socket_;

    std::shared_ptr<request_handler> req_handler_ptr_;
    std::shared_ptr<logger> logger_ptr_;

    size_t header_;
    // store incoming client raw request
    std::vector<uint8_t> buffer_;
    // store a wrapper for the request and a reference for one reply (of replies) at time
    communication::message msg_;
    // store the processed replies for a given request
    communication::message_queue replies_;

    // This timer is used to manage user disconnection that are not automatically detected
    boost::asio::steady_timer timeout_timer_;
    bool timed_out = false;

    // store the user information to handle him session
    user user_;

    void schedule_timeout();

    void shutdown();

    void log_read();

    void log_write(boost::system::error_code const &e);

    void handle_completion(boost::system::error_code const &e);

    void write_response(boost::system::error_code const &e);

    void handle_request(boost::system::error_code const &e);

    void read_request(boost::system::error_code const &e);

public:

    explicit connection(boost::asio::io_context &io,
                        boost::asio::ssl::context &ctx,
                        std::shared_ptr<logger> logger_ptr,
                        std::shared_ptr<request_handler> req_handler);

    ssl_socket &socket();

    void start();
};


#endif //REMOTE_BACKUP_M1_SERVER_CONNECTION_H
