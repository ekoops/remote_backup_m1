#include "server.h"
#include <boost/thread/thread.hpp>
#include <boost/bind/bind.hpp>
#include <vector>

namespace fs = boost::filesystem;

/**
 * Construct a new server instance, setting up the necessary
 * handler for signals handling, the acceptor for incoming
 * connections and SSL context parameters.
 *
 * @param vm a variable map containing all program options to set up server instance
 * @return a new constructed server instance
 */
server::server(boost::program_options::variables_map const &vm)
        : thread_pool_size_{vm["threads"].as<std::size_t>()},
          signals_{io_},
          acceptor_{io_},
          ctx_{boost::asio::ssl::context::sslv23}, // set generic ssl/tls version
          new_connection_ptr_{},
          logger_ptr_{std::make_shared<logger>(vm["logger-file"].as<fs::path>())},
          req_handler_ptr_{std::make_shared<request_handler>(
                  vm["backup-root"].as<fs::path>(),
                  vm["credentials-file"].as<fs::path>()
          )} {
    // Register to handle the signals that indicate when the server should exit.
    this->signals_.add(SIGINT);
    this->signals_.add(SIGTERM);
#if defined(SIGQUIT)
    this->signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
    this->signals_.async_wait(boost::bind(&server::handle_stop, this));

    std::string address = vm["address"].as<std::string>();
    std::string service = vm["service"].as<std::string>();
    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    boost::asio::ip::tcp::resolver resolver(io_);
    boost::asio::ip::tcp::endpoint endpoint =
            *resolver.resolve(address, service).begin();
    this->acceptor_.open(endpoint.protocol());
    this->acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    this->acceptor_.bind(endpoint);
    this->acceptor_.listen();

    // Setting up ssl context parameters
    this->ctx_.set_options(
            boost::asio::ssl::context::default_workarounds  // ssl bug workarounds
            | boost::asio::ssl::context::no_sslv2             // disable support for sslv2
            | boost::asio::ssl::context::single_dh_use);       // Always create a new key when using tmp_dh parameters
    this->ctx_.use_certificate_chain_file("../files/certs/server-cert.pem");  // loading server pem certificate
    this->ctx_.use_private_key_file("../files/certs/server-key.pem", boost::asio::ssl::context::pem);
    this->ctx_.use_tmp_dh_file("../files/certs/dh2048.pem");

    start_accept();
}

/**
 * Creates a thread pool composed of thread_pool_size threads.
 * The threads will call the run method on the io_context on
 * which the server is defined to process the enqueued completion
 * handlers (connection tasks). This is a blocking due to the fact
 * that the created threads are joined.
 *
 * @return void
 */
void server::run() {
    // Create a pool of threads to run all of the io_contexts.
    std::vector<std::thread> threads;
    threads.reserve(this->thread_pool_size_);
    for (int i = 0; i < this->thread_pool_size_; i++) {
        threads.emplace_back(boost::bind(&boost::asio::io_context::run, &io_));
    }

    // Wait for all threads in the pool to exit.
    for (auto &t: threads) t.join();
}

/**
 * This method allows to start accept on a new created
 * SSL socket
 *
 * @return void
 */
void server::start_accept() {
    this->new_connection_ptr_.reset(new connection(
            this->io_,
            this->ctx_,
            this->logger_ptr_,
            this->req_handler_ptr_
    ));
    this->acceptor_.async_accept(
            this->new_connection_ptr_->socket().lowest_layer(),
            boost::bind(
                    &server::handle_accept, this, boost::asio::placeholders::error
            )
    );

}

/**
 * This method is called after a client connection. It
 * starts the request-response cycle for a client and
 * allows the server to restart the acceptance phase
 *
 * @return void
 */
void server::handle_accept(const boost::system::error_code &e) {
    if (!e) this->new_connection_ptr_->start();
    start_accept();
}

void server::handle_stop() {
    this->io_.stop();
}
