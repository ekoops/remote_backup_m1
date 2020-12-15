#include "message_queue.h"

using namespace communication;
namespace fs = boost::filesystem;

size_t const message_queue::CHUNK_SIZE = 4096;

/**
 * Construct a message_queue instance with a specific message type
 *
 * @param msg_type the message type of the message_queue
 * @return a new constructed message_queue instance
 */
message_queue::message_queue(
        MSG_TYPE msg_type
) : msg_type_{msg_type}
, msgs_queue_{std::queue<message>{}}
, err_type_ {ERR_TYPE::ERR_NONE} {
    this->msgs_queue_.emplace(this->msg_type_);
}

/**
 * Allow to assign a new TLV tag to the last message in
 * the queue. If the dimension of the last message plus
 * the new TLV tag dimension is greater than the CHUNK
 * size, then a new message is constructed and added
 * to the queue with the specified TLV tag.
 *
 * @param tlv_type the TLV type
 * @param length the TLV value field length
 * @param buffer a pointer to the data that has to be copied in the TLV value field
 * @return void
 */
void message_queue::add_TLV(TLV_TYPE tlv_type, size_t length, const char *buffer) {
    // there is at least an element so empty check is useless
    auto &last = this->msgs_queue_.back();
    if (last.size() + 1 + 4 + length <= CHUNK_SIZE) {
        last.add_TLV(tlv_type, length, buffer);
    } else {
        message msg{this->msg_type_};
        msg.add_TLV(tlv_type, length, buffer);
        this->msgs_queue_.push(msg);
    }
    if (tlv_type == communication::TLV_TYPE::ERROR) {
        std::string error_str {buffer, length};
        this->err_type_ = static_cast<ERR_TYPE>(std::stoi(error_str));
    }
}

void message_queue::pop() {
    this->msgs_queue_.pop();
}

message message_queue::front() {
    return this->msgs_queue_.front();
}

bool message_queue::empty() {
    return this->msgs_queue_.empty();
}

MSG_TYPE message_queue::msg_type() const {
    return this->msg_type_;
}

ERR_TYPE message_queue::err_type() const {
    return this->err_type_;
}