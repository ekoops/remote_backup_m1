#ifndef REMOTE_BACKUP_M1_SERVER_SERVER_H
#define REMOTE_BACKUP_M1_SERVER_SERVER_H


#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include "connection.h"
#include "../utilities/logger.h"

/*
 * This class provides an abstraction of the entire
 * backup server. A thread pool is used to manage
 * all incoming connection with an async approach.
 */
class server : private boost::noncopyable {
    std::size_t thread_pool_size_;
    boost::asio::io_context io_;
    boost::asio::ssl::context ctx_;
    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;
    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::shared_ptr<connection> new_connection_ptr_;
    std::shared_ptr<request_handler> req_handler_ptr_;
    std::shared_ptr<logger> logger_ptr_;

public:
    explicit server(boost::program_options::variables_map const& vm);
    void run();

private:
    void start_accept();
    void handle_accept(const boost::system::error_code &e);
    void handle_stop();
};


#endif //REMOTE_BACKUP_M1_SERVER_SERVER_H
