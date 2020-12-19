#ifndef REMOTE_BACKUP_M1_TYPES_H
#define REMOTE_BACKUP_M1_TYPES_H


namespace communication {
    /*
     * These enums define the allowed message type, the allowed
     * TLV type, the possible server error response and the
     * communication result for logging.
     */
    enum MSG_TYPE {
        NONE = 0,
        CREATE = 1,
        UPDATE = 2,
        ERASE = 3,
        LIST = 4,
        AUTH = 5,
        RETRIEVE = 6,
        KEEP_ALIVE = 7
    };

    enum TLV_TYPE {
        USRN = 0,
        PSWD = 1,
        ITEM = 2,
        END = 3,
        OK = 4,
        ERROR = 5,
        CONTENT = 6
    };

    enum ERR_TYPE {
        ERR_NONE = 0,
        ERR_NO_CONTENT = 1,
        ERR_MSG_TYPE_REJECTED = 2,
        ERR_CREATE_NO_ITEM = 101,
        ERR_CREATE_NO_CONTENT = 102,
        ERR_CREATE_ALREADY_EXIST = 103,
        ERR_CREATE_FAILED = 104,
        ERR_CREATE_NO_MATCH = 105,
        ERR_UPDATE_NO_ITEM = 201,
        ERR_UPDATE_NO_CONTENT = 202,
        ERR_UPDATE_NOT_EXIST = 203,
        ERR_UPDATE_ALREADY_UPDATED = 204,
        ERR_UPDATE_FAILED = 205,
        ERR_UPDATE_NO_MATCH = 206,
        ERR_ERASE_NO_ITEM = 301,
        ERR_ERASE_NO_MATCH = 302,
        ERR_ERASE_FAILED = 303,
        ERR_LIST_FAILED = 401,
        ERR_AUTH_NO_USRN = 501,
        ERR_AUTH_NO_PSWD = 502,
        ERR_AUTH_FAILED = 503,
        ERR_RETRIEVE_FAILED = 601
    };

    // communication result for logging
    enum CONN_RES {
        CONN_NONE = 0,
        CONN_OK = 1,
        CONN_ERR = 2
    };
}

#endif //REMOTE_BACKUP_M1_TYPES_H
