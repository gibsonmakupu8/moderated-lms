#pragma once

#include <memory>

#include "lms/PasswordHasher.hpp"
#include "lms/Repositories.hpp"
#include "lms/User.hpp"

namespace lms {

class AuthService {
public:
    AuthService(IUserRepository& users, const IPasswordHasher& hasher);

    // Returns the logged-in user (a Student, Lecturer, ... object). Every failure produces the same
    // message so the response does not reveal whether the username exists.
    std::unique_ptr<User> login(const std::string& username, const std::string& password) const;
    void changePassword(const User& user, const std::string& oldPassword, const std::string& newPassword);

private:
    IUserRepository& users_;
    const IPasswordHasher& hasher_;
    std::string dummyHash_;  // verified when the username is unknown, to keep response time similar
};

}  // namespace lms
