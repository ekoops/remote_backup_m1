#ifndef REMOTE_BACKUP_M1_TOOLS_H
#define REMOTE_BACKUP_M1_TOOLS_H
#include <string>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/uuid/detail/md5.hpp>

// Utility static methods
struct tools {
    static std::string create_sign(
            boost::filesystem::path const &relative_path,
            std::string const &digest
    );

    static std::pair<boost::filesystem::path, std::string> split_sign(std::string const &sign);

    static std::pair<bool, std::vector<std::string>> match_and_parse(
            boost::regex const &regex,
            std::string const &str
    );

    static std::string MD5_hash(std::string const &str);

    static std::string MD5_hash(
            boost::filesystem::path const &absolute_path,
            boost::filesystem::path const &relative_path
    );

    static std::string SHA512_hash(std::string const &str);

    static bool verify_password(
            boost::filesystem::path const &credentials_path,
            std::string const &username,
            std::string const &password
    );

private:
    static std::string MD5_to_string(boost::uuids::detail::md5::digest_type const &digest);
};


#endif //REMOTE_BACKUP_M1_TOOLS_H
