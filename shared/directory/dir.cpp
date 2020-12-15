#include "dir.h"

using namespace directory;


/**
 * Construct a new directory instance.
 *
 * @param path Path associated with the created instance.
 * @param concurrent_accessed A boolean value specifying if the directory is accessed in concurrent mode
 * @return a new constructed directory instance
 */
template<typename R>
dir<R>::dir(boost::filesystem::path path,
            bool concurrent_accessed)
        : path_{std::move(path)}, concurrent_accessed_{concurrent_accessed} {}

/**
 * Construct a new directory instance shared_ptr.
 *
 * @param path Path associated with the created instance.
 * @param concurrent_accessed A boolean value specifying if the directory is accessed in concurrent mode
 * @return a new constructed directory instance shared_ptr
 */
template<typename R>
std::shared_ptr<dir<R>> dir<R>::get_instance(boost::filesystem::path path, bool concurrent_accessed) {
    return std::shared_ptr<dir<R>>(new dir<R>{std::move(path), concurrent_accessed});
}

/**
 * Insert or assign a new directory entry.
 *
 * @param path Path of the entry.
 * @param rsrc Resource associated to path.
 * @return true if a new entry has been inserted, false if an assigment has been performed.
 */
template<typename R>
bool dir<R>::insert_or_assign(boost::filesystem::path const &path, R const &rsrc) {
    std::unique_lock ul{this->m_, std::defer_lock};
    if (concurrent_accessed_) ul.lock();
    return this->content_.insert_or_assign(path, rsrc).second;
}

/**
 * Erase a directory entry.
 *
 * @param path Path of the entry.
 * @return true if an entry has been deleted, false otherwise.
 */
template<typename R>
bool dir<R>::erase(boost::filesystem::path const &path) {
    std::unique_lock ul{this->m_, std::defer_lock};
    if (concurrent_accessed_) ul.lock();
    return this->content_.erase(path) == 1;
}

/**
 * Check if directory contains a specified path.
 *
 * @param path Path of the entry.
 * @return true if an entry associated with the specified path has been found, false otherwise.
 */
template<typename R>
bool dir<R>::contains(boost::filesystem::path const &path) const {
    std::unique_lock ul{this->m_, std::defer_lock};
    if (concurrent_accessed_) ul.lock();
    return this->content_.find(path) != this->content_.end();
}

/**
 * Provide the directory associated path
 *
 * @return the directory associated path.
 */
template<typename R>
boost::filesystem::path const &dir<R>::path() const {
    return this->path_;
}

/**
 * Provide the provided path associated resource
 *
 * @param path Path of the entry.
 * @return an std::optional<R> containing the provided path associated resource
 * or std::nullopt if directory doesn't contain an entry associated to path
 */
template<typename R>
std::optional<R> dir<R>::rsrc(boost::filesystem::path const &path) const {
    std::unique_lock ul{this->m_, std::defer_lock};
    if (concurrent_accessed_) ul.lock();
    auto it = this->content_.find(path);
    return it != this->content_.end() ? std::optional<R>{it->second} : std::nullopt;

}

/**
 * Allows to execute a specified function on each directory entry
 *
 * @param fn Function that have to be executed on each directory entry.
 * @return void
 */
template<typename R>
void dir<R>::for_each(std::function<void(std::pair<boost::filesystem::path, R> const &)> const &fn) const {
    std::unique_lock ul{this->m_, std::defer_lock};
    if (concurrent_accessed_) ul.lock();
    auto it = this->content_.cbegin();
    while (it != this->content_.cend()) fn(*it++);
}

/**
 * Allows to erase all directory entries
 *
 * @return void
 */
template<typename R>
void dir<R>::clear() {
    std::unique_lock ul{this->m_, std::defer_lock};
    if (concurrent_accessed_) ul.lock();
    this->content_.clear();
}