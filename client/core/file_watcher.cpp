#include <utility>
#include "file_watcher.h"
#include "../../shared/utilities/tools.h"

namespace fs = boost::filesystem;

/**
 * Construct a file_watcher instance with the associated watched directory
 * and a scheduler for server update scheduling.
 *
 * @param dir_ptr std::shared_ptr to the watched directory
 * @param scheduler_ptr std::shared_ptr to an operation scheduler
 * @param wait_time file_watcher refresh rate in milliseconds
 * @return a new constructed file_watcher instance
 */
file_watcher::file_watcher(
        std::shared_ptr<directory::dir<directory::c_resource>> dir_ptr,
        std::shared_ptr<scheduler> scheduler_ptr,
        std::chrono::milliseconds wait_time
) : dir_ptr_{std::move(dir_ptr)}, scheduler_ptr_{std::move(scheduler_ptr)}, wait_time_{wait_time} {
    std::cout << " \u25CC Scanning directory..." << std::endl;
    size_t watched_dir_length = this->dir_ptr_->path().size();
    for (auto &de : fs::recursive_directory_iterator(dir_ptr_->path())) {
        fs::path const &absolute_path = de.path();
        if (fs::is_regular_file(absolute_path)) {
            boost::filesystem::path relative_path{absolute_path.generic_path().string().substr(watched_dir_length)};
            this->dir_ptr_->insert_or_assign(relative_path, directory::c_resource{
                    boost::indeterminate,
                    false,
                    tools::MD5_hash(absolute_path, relative_path)
            });
        }
    }
}

/**
 * Allow to start to watch the associated directory to sync
 * it with the internal representation and the backup server directory
 *
 * @return void
 */
void file_watcher::start() {
    fs::path const &root = this->dir_ptr_->path();
    size_t watched_dir_length = root.size();

    this->scheduler_ptr_->sync();

    while (this->running_) {
        std::this_thread::sleep_for(this->wait_time_);

        this->dir_ptr_->for_each([&root, this](std::pair<fs::path, directory::c_resource> const &pair) {
            if (fs::exists(root / pair.first)) return;
            fs::path const &relative_path = pair.first;
            directory::c_resource const &rsrc = pair.second;

            if (!boost::indeterminate(rsrc.synced())) {
                // TODO eliminare l'if
                if (rsrc.exist_on_server()) {
                    this->scheduler_ptr_->erase(relative_path, rsrc.digest());
                }
            }
        });

        for (auto &file : fs::recursive_directory_iterator(root)) {
            fs::path const &absolute_path = file.path();
            if (fs::is_regular_file(absolute_path)) {
                fs::path relative_path{absolute_path.generic_path().string().substr(watched_dir_length)};
                std::string digest = tools::MD5_hash(absolute_path, relative_path);

                // if doesn't exists
                if (!this->dir_ptr_->contains(relative_path)) {
                    this->scheduler_ptr_->create(relative_path, digest);
                } else {
                    directory::c_resource rsrc = this->dir_ptr_->rsrc(relative_path).value();
                    if (rsrc.synced() == true) {
                        if (rsrc.digest() != digest) {
                            this->scheduler_ptr_->update(relative_path, digest);
                        }
                    } else if (rsrc.synced() == false) {
                        if (rsrc.exist_on_server()) {
                            this->scheduler_ptr_->update(relative_path, digest);
                        } else this->scheduler_ptr_->create(relative_path, digest);
                    }
                }
            }
        }
    }
}