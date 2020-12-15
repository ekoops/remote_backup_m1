#ifndef REMOTE_BACKUP_M1_CLIENT_RESOURCE_H
#define REMOTE_BACKUP_M1_CLIENT_RESOURCE_H

#include <string>
#include <boost/logic/tribool.hpp>
#include <utility>
#include <iostream>

/*
 * synced:
 *      true:             client is synced with server on this resource
 *      false:            client is not synced with server on this resource = failed synchronization
 *      indeterminate:    waiting for server response or not already processed server response
 *
 * exist_on_server:
 *      true:           the client knows that the resource exists on server
 *      false:          the client knows that the resource doesn't exist on server
 *
 * digest: digest value of resource
 */

namespace directory {
    /*
     * This class represent details about a directory object with respect to the client vision
     * of the resource state on server
     */
    class c_resource {
        boost::logic::tribool synced_;
        bool exist_on_server_;
        std::string digest_;
    public:
        c_resource(boost::logic::tribool synced, bool exist_on_server, std::string digest);

        // setters and getters section

        c_resource& synced(boost::logic::tribool const &synced);

        [[nodiscard]] boost::logic::tribool synced() const;

        c_resource& exist_on_server(bool exist_on_server);

        [[nodiscard]] bool exist_on_server() const;

        c_resource& digest(std::string digest);

        [[nodiscard]] std::string const& digest() const;
    };
    // TODO cancellare l'operazione di redirezione
    std::ostream& operator<<(std::ostream& os, directory::c_resource const& rsrc);
}


#endif //REMOTE_BACKUP_M1_CLIENT_RESOURCE_H
