#include "logger.h"
#include "../../shared/communication/tlv_view.h"
#include "../../shared/utilities/tools.h"

namespace fs = boost::filesystem;
using namespace communication;

/**
 * Create a logger instance with a given log file path
 *
 * @param path the path on which the created logger has to log
 * @return void
 */
logger::logger(fs::path const &path) : ofs_{fs::ofstream{path, std::ios_base::app}} {
    this->msg_type_str_map_ = {
            {NONE,       "-"},
            {CREATE,     "CREATE"},
            {UPDATE,     "UPDATE"},
            {ERASE,      "ERASE"},
            {AUTH,       "AUTH"},
            {KEEP_ALIVE, "KEEP_ALIVE"},
            {LIST,       "LIST"},
            {RETRIEVE,   "RETRIEVE"}
    };

    this->tlv_type_str_map_ = {
            {USRN,    "USRN"},
            {PSWD,    "PSWD"},
            {ITEM,    "ITEM"},
            {END,     "END"},
            {OK,      "OK"},
            {ERROR,   "ERROR"},
            {CONTENT, "CONTENT"}
    };

    this->err_type_str_map_ = {
            {ERR_NONE,                   "OK"},
            {ERR_NO_CONTENT,             "ERR_NO_CONTENT"},
            {ERR_MSG_TYPE_REJECTED,      "ERR_MSG_TYPE_REJECTED"},
            {ERR_CREATE_NO_ITEM,         "ERR_CREATE_NO_ITEM"},
            {ERR_CREATE_NO_CONTENT,      "ERR_CREATE_NO_CONTENT"},
            {ERR_CREATE_ALREADY_EXIST,   "ERR_CREATE_ALREADY_EXIST"},
            {ERR_CREATE_FAILED,          "ERR_CREATE_FAILED"},
            {ERR_CREATE_NO_MATCH,        "ERR_CREATE_NO_MATCH"},
            {ERR_UPDATE_NO_ITEM,         "ERR_UPDATE_NO_ITEM"},
            {ERR_UPDATE_NO_CONTENT,      "ERR_UPDATE_NO_CONTENT"},
            {ERR_UPDATE_NOT_EXIST,       "ERR_UPDATE_NOT_EXIST"},
            {ERR_UPDATE_ALREADY_UPDATED, "ERR_UPDATE_ALREADY_UPDATED"},
            {ERR_UPDATE_FAILED,          "ERR_UPDATE_FAILED"},
            {ERR_UPDATE_NO_MATCH,        "ERR_UPDATE_NO_MATCH"},
            {ERR_ERASE_NO_ITEM,          "ERR_ERASE_NO_ITEM"},
            {ERR_ERASE_NO_MATCH,         "ERR_ERASE_NO_MATCH"},
            {ERR_ERASE_FAILED,           "ERR_ERASE_FAILED"},
            {ERR_LIST_FAILED,            "ERR_LIST_FAILED"},
            {ERR_AUTH_NO_USRN,           "ERR_AUTH_NO_USRN"},
            {ERR_AUTH_NO_PSWD,           "ERR_AUTH_NO_PSWD"},
            {ERR_AUTH_FAILED,            "ERR_AUTH_FAILED"},
            {ERR_RETRIEVE_FAILED,        "ERR_RETRIEVE_FAILED"}
    };

    this->conn_res_str_map_ = {
            {CONN_NONE, "-"},
            {CONN_OK,   "OK"},
            {CONN_ERR,  "ERR"}
    };
}

/**
 * Log the given message related to the given user
 *
 * @param usr the user to whom the message refers
 * @param message the message that has to be logged
 * @return void
 */
void logger::log(user const &usr, std::string const &message) {
    std::string today = logger::get_time();
    std::string const &username = usr.username();
    std::string const &ip = usr.ip();

    std::ostringstream log;
    log << '[' << today << "]["
        << username << (username.empty() ? "" : "@") << ip << "]["
        << message << ']' << std::endl;

    this->ofs_ << log.str();
}

void logger::log(user const &usr, communication::message const &message) {
    std::ostringstream log;
    auto msg_type_str = this->msg_type_str_map_.find(message.msg_type())->second;
    log << usr.username() << ":" << msg_type_str << std::endl;
    communication::tlv_view view{message};
    while (view.next_tlv()) {
        auto tlv_type_str = this->tlv_type_str_map_.find(view.tlv_type())->second;
        log << "\tT: " << tlv_type_str;
        log << "\tL: " << view.length();
        if (view.tlv_type() != communication::TLV_TYPE::CONTENT) {
            std::string str{view.cbegin(), view.cend()};
            if (view.tlv_type() == communication::ITEM) {
                str = tools::split_sign(str).first.string();
            } else if (view.tlv_type() == communication::ERROR) {
                str = this->err_type_str_map_.find(
                        static_cast<const ERR_TYPE>(stoi(str))
                )->second;
            }
            log << "\tV: " << str;
        }
        log << std::endl;
    }
    std::cout << log.str();
}

/**
 * Log the result of a communication instance for the given user in the format
 * [today][username@ip][TYPE: MSG_TYPE RES: ERR_TYPE  CON: OK|ERR]

 *
 * @param usr the user to whom the message refers
 * @param msg_type the message type of the communication instance
 * @param message_result the message processing result
 * @param connection_result the state of connection during communication
 * @return void
 */
void logger::log(
        user const &usr,
        MSG_TYPE msg_type,
        ERR_TYPE message_result,
        CONN_RES connection_result
) {
    std::string today = logger::get_time();
    std::string const &username = usr.username();
    std::string const &ip = usr.ip();
    auto msg_type_str = this->msg_type_str_map_.find(msg_type)->second;
    auto msg_res_str = this->err_type_str_map_.find(message_result)->second;
    auto conn_res_str = this->conn_res_str_map_.find(connection_result)->second;
    std::ostringstream log;
    log << '[' << today << "]["
        << username << (username.empty() ? "" : "@") << ip << "]["
        << "TYPE: " << msg_type_str
        << " RES: " << msg_res_str
        << " CONN: " << conn_res_str << ']' << std::endl;
    this->ofs_ << log.str();
}

/*
 * An internal logger method used to obtain the current time
 * in a string format
 *
 * @return a string representation of the current time
 */
std::string logger::get_time() {
    using namespace boost::posix_time;
    ptime today{second_clock::universal_time()};
    std::string today_string{to_iso_extended_string(today)};
    std::istringstream iss{today_string};
    std::string today_substring;
    std::getline(iss, today_substring, ',');
    return today_substring;
}