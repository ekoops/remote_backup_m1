#include <boost/bind/bind.hpp>
#include <vector>
#include "connection.h"

#define TIMEOUT 60

connection::connection(
        boost::asio::io_context &io,
        boost::asio::ssl::context &ctx,
        std::shared_ptr<logger> logger_ptr,
        std::shared_ptr<request_handler> req_handler)
        : strand_(boost::asio::make_strand(io)),
          socket_{strand_, ctx},
          logger_ptr_{std::move(logger_ptr)},
          req_handler_ptr_{std::move(req_handler)},
          buffer_(communication::message_queue::CHUNK_SIZE),
          timeout_timer_{strand_, std::chrono::seconds(TIMEOUT)} {
}

/*
 * Return the SSL socket
 */
ssl_socket &connection::socket() {
    return this->socket_;
}

/*
 * Allow to schedule a timeout timer for handling not automatically
 * detected disconnected client
 */
void connection::schedule_timeout() {
    try {
        this->timeout_timer_.expires_after(boost::asio::chrono::seconds{TIMEOUT});
        this->timeout_timer_.async_wait(
                [this](boost::system::error_code const &e) {
                    if (!e) {
                        this->timed_out = true;
                        this->shutdown();
                    } else if (e != boost::asio::error::operation_aborted) {
                        return schedule_timeout();
                    }
                }
        );
    }
    catch (boost::system::system_error &e) {
        std::cerr << "Failed to set timeout timer" << std::endl;
    }
}

/*
 * Allows to gracefully shutdown the client connection
 */
void connection::shutdown() {
    this->req_handler_ptr_->streams().erase_stream(this->user_);
    this->timeout_timer_.cancel();
    this->logger_ptr_->log(this->user_, "Shutdown");
    boost::system::error_code ignored_ec;
    this->socket_.shutdown(ignored_ec);
    this->socket_.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
}

/*
 * An helper method to log the result on the read phase
 */
void connection::log_read() {
    this->logger_ptr_->log(
            this->user_,
            communication::MSG_TYPE::NONE,
            communication::ERR_TYPE::ERR_NONE,
            communication::CONN_RES::CONN_ERR
    );
}

/*
 * An helper method to log the result on the write phase
 */
void connection::log_write(boost::system::error_code const &e) {
    this->logger_ptr_->log(
            this->user_,
            this->replies_.msg_type(),
            this->replies_.err_type(),
            !e
            ? communication::CONN_RES::CONN_OK
            : communication::CONN_RES::CONN_ERR
    );
}

/*
 * Handle the last part of the communication logging the results,
 * shutdowning the connection or starting to read another request
 * depending on the presence of errors.
 */
void connection::handle_completion(boost::system::error_code const &e) {
    if (this->timed_out) return;
    this->log_write(e);
    if (e) this->shutdown();
    else this->read_request(e);
}

/*
 * Writes a single message in replies and then recall itself
 * until the replies queue is empty
 */
void connection::write_response(boost::system::error_code const &e) {
    if (this->timed_out) return;
    if (e) {
        this->log_write(e);
        return this->shutdown();
    }
    this->header_ = this->replies_.front().size();
    std::cout << "<<<<<<<<<<<<RESPONSE>>>>>>>>>>>>" << std::endl;
    std::cout << "HEADER: " << this->header_ << std::endl;
    boost::asio::async_write(
            this->socket_,
            boost::asio::buffer(&this->header_, sizeof(this->header_)),
            [self = shared_from_this()](boost::system::error_code const &e, size_t /**/) {
                if (self->timed_out) return;
                if (e) {
                    self->log_write(e);
                    return self->shutdown();
                }
                self->msg_ = self->replies_.front();
                self->replies_.pop();
                std::cout << self->msg_ << std::endl;
                boost::asio::async_write(
                        self->socket_,
                        self->msg_.buffer(),
                        boost::bind(
                                self->replies_.empty()
                                ? &connection::handle_completion
                                : &connection::write_response,
                                self->shared_from_this(),
                                boost::asio::placeholders::error
                        )
                );
            }
    );
}

/*
 * Handle the provided client request producing necessary replies.
 */
void connection::handle_request(boost::system::error_code const &e) {
    if (this->timed_out) return;
    if (e) {
        this->log_read();
        return this->shutdown();
    }
    try {
        this->timeout_timer_.cancel();
        this->msg_ = communication::message{
                std::make_shared<std::vector<uint8_t>>(
                        this->buffer_.begin(),
                        std::next(this->buffer_.begin(), this->header_)
                )
        };
    } catch (std::exception const &e) {
        std::cout << e.what() << std::endl;
        return this->shutdown();
    }

    std::cout << "<<<<<<<<<<<<REQUEST>>>>>>>>>>>>" << std::endl;
    std::cout << this->msg_;

    this->req_handler_ptr_->handle_request(
            this->msg_,
            this->replies_,
            this->user_
    );
    this->write_response(boost::system::error_code{});
}

/*
 * Reads the request header and payload. It manages errors
 * and possible connection timeouts.
 */
void connection::read_request(boost::system::error_code const &e) {
    if (this->timed_out) return;
    if (e) {
        this->log_read();
        return this->shutdown();
    }
    this->schedule_timeout();
    boost::asio::async_read(
            this->socket_,
            boost::asio::buffer(&this->header_, sizeof(this->header_)),
            [self = shared_from_this()](boost::system::error_code const &e, size_t /**/) {
                if (self->timed_out) return;
                if (e) {
                    self->log_read();
                    return self->shutdown();
                }
                self->schedule_timeout();
                boost::asio::async_read(
                        self->socket_,
                        boost::asio::buffer(self->buffer_, self->header_),
                        boost::bind(
                                &connection::handle_request,
                                self->shared_from_this(),
                                boost::asio::placeholders::error
                        )
                );
            }
    );
}

/*
 * Starts the interaction with a client scheduling
 * the timer and the handshake request
 */
void connection::start() {
    this->user_.ip(this->socket_.lowest_layer().remote_endpoint().address().to_string());
    this->logger_ptr_->log(this->user_, "Accepted connection");
    this->socket_.lowest_layer().set_option(boost::asio::socket_base::keep_alive(true));
    this->schedule_timeout();
    this->socket_.async_handshake(boost::asio::ssl::stream_base::server,
                                  boost::bind(&connection::read_request,
                                              shared_from_this(),
                                              boost::asio::placeholders::error));
}