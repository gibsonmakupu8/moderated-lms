#include "lms/AuthService.hpp"

#include "lms/Validation.hpp"

namespace lms {

AuthService::AuthService(IUserRepository& users, const IPasswordHasher& hasher)
    : users_(users), hasher_(hasher), dummyHash_(hasher.hash("dummy-password-for-timing-1")) {}

std::unique_ptr<User> AuthService::login(const std::string& username, const std::string& password) const {
    static const char* const kFailure = "Invalid username or password.";
    std::optional<UserRecord> record;
    if (!username.empty() && username.size() <= 64) record = users_.findByUsername(trim(username));
    if (!record) {
        hasher_.verify(dummyHash_, password);
        throw AuthenticationError(kFailure);
    }
    if (!hasher_.verify(record->passwordHash, password) || !record->active) throw AuthenticationError(kFailure);
    return UserFactory::create(*record);
}

void AuthService::changePassword(const User& user, const std::string& oldPassword, const std::string& newPassword) {
    auto record = users_.findById(user.id());
    if (!record) throw NotFoundError("Your account no longer exists.");
    if (!hasher_.verify(record->passwordHash, oldPassword)) throw AuthenticationError("The current password is incorrect.");
    requirePasswordPolicy(newPassword);
    record->passwordHash = hasher_.hash(newPassword);
    users_.update(*record);
}

}  // namespace lms
