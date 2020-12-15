#include "open_streams.h"

namespace fs = boost::filesystem;

/*
 * Allows to retrieve an already opened stream or, if it
 * doesn't exist, a new created one. The stream is stored
 * internally.
 *
 * @param user the user to identify the requested stream
 * @param path the path referring to the file the user wants to handle
 * @return a pair composed of the following two parts:
 * @return - an iterator to the opened stream
 * @return - a bool indicating if the stream is already opened and simply retrieved or created and retrieved
 */
get_stream_result open_streams::get_stream(
        user const &user,
        fs::path const &path
) {
    std::unique_lock ul{this->m_};
    return this->streams_.emplace(
            user,
            std::make_shared<fs::ofstream>(path, std::ios_base::binary | std::ios_base::app)
    );
}

/*
 * Allows to retrieve an already opened stream or, if it
 * doesn't exist, a new created one. The stream is stored
 * internally.
 *
 * @param user the user identifying the stream that has to be removed
 * @return void
 */
void open_streams::erase_stream(user const &user) {
    std::unique_lock ul{this->m_};
    this->streams_.erase(user);
}