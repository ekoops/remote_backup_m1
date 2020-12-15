#include "auth_data.h"

/**
 * Construct a user instance.
 *
 * @param username the user username
 * @param password the user password
 * @return a new constructed user instance
 */
auth_data::auth_data(std::string username, std::string password)
: username_ {std::move(username)}
, password_ {std::move(password)} {}

/**
* Getter for username field.
*
* @return the username field reference
*/
[[nodiscard]] std::string const& auth_data::username() const {
    return this->username_;
}

/**
* Getter for password field.
*
* @return the password field reference
*/
[[nodiscard]] std::string const& auth_data::password() const {
    return this->password_;
}

/**
* Allow to verify if the user is authenticated.
*
* @return true if the user is authenticated, false otherwise
*/
[[nodiscard]] bool auth_data::authenticated() const {
    return this->authenticated_;
}

/**
* Allow to set the user authentication state
*
* @param authenticated the new user authentication state
* @return void
*/
void auth_data::authenticated(bool authenticated) {
    this->authenticated_ = authenticated;
}
