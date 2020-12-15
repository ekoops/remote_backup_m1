#ifndef REMOTE_BACKUP_M1_SERVER_OPEN_STREAMS_H
#define REMOTE_BACKUP_M1_SERVER_OPEN_STREAMS_H

#include <unordered_map>
#include "user.h"


typedef std::pair<
        std::unordered_map<user, std::shared_ptr<boost::filesystem::ofstream>>::iterator,
        bool
> get_stream_result;

/*
 * This class allows handle open streams in a cuncurrent
 * way.
 */
class open_streams {
    std::unordered_map<user, std::shared_ptr<boost::filesystem::ofstream>> streams_;
    std::mutex m_;
public:
    get_stream_result get_stream(user const &user, boost::filesystem::path const &path);
    void erase_stream(user const &user);
};


#endif //REMOTE_BACKUP_M1_SERVER_OPEN_STREAMS_H
