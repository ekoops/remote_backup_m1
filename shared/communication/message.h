#ifndef REMOTE_BACKUP_M1_MESSAGE_H
#define REMOTE_BACKUP_M1_MESSAGE_H


#include <iostream>
#include <vector>
#include <cstdint>
#include <mutex>
#include <boost/asio/ip/tcp.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/serialization/vector.hpp>
#include "types.h"

namespace communication {
    /*
     * This class allows to incrementally create a message
     * for handle server communication
     */
    class message {
        std::shared_ptr<std::vector<uint8_t>> raw_msg_ptr_;
    public:
        explicit message(MSG_TYPE msg_type = MSG_TYPE::NONE);

        explicit message(std::shared_ptr<std::vector<uint8_t>> raw_msg_ptr);

        void add_TLV(TLV_TYPE tlv_type, size_t length = 0, char const *buffer = nullptr);

        [[nodiscard]] std::shared_ptr<std::vector<uint8_t>> raw_msg_ptr() const;

        [[nodiscard]] MSG_TYPE msg_type() const;

        [[nodiscard]] boost::asio::mutable_buffer buffer() const;

        [[nodiscard]] size_t size() const;

        void resize(size_t length);

        bool operator==(message const &other) const;
    };

    std::ostream &operator<<(std::ostream &os, communication::message const &msg);
}


#endif //REMOTE_BACKUP_M1_MESSAGE_H
