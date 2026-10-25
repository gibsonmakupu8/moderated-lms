// Abstract User with four concrete roles (polymorphism), created through UserFactory (Factory pattern).
#pragma once

#include <memory>
#include <set>
#include <string>

#include "lms/Models.hpp"

namespace lms {

enum class Permission {
    ManageUsers,      // accounts
    ManageStructure,  // departments, courses, lecturer assignment, enrolment
    ViewAssignments,
    CreateAssignment,
    GrantExtension,
    SubmitWork,
    MarkSubmissions,
    ModerateMarks,
    ReleaseMarks,
    ViewReleasedResults
};

std::string toString(Permission permission);

class User {
public:
    explicit User(UserRecord record) : record_(std::move(record)) {}
    virtual ~User() = default;

    Id id() const { return record_.id; }
    const std::string& username() const { return record_.username; }
    const std::string& fullName() const { return record_.fullName; }
    const std::optional<Id>& departmentId() const { return record_.departmentId; }

    virtual Role role() const = 0;
    virtual std::set<Permission> permissions() const = 0;  // role-based access control
    bool can(Permission permission) const { return permissions().count(permission) > 0; }
    std::string roleName() const { return displayName(role()); }

protected:
    UserRecord record_;
};

class SystemAdministrator : public User {
public:
    using User::User;
    Role role() const override { return Role::Admin; }
    std::set<Permission> permissions() const override;
};

class Lecturer : public User {
public:
    using User::User;
    Role role() const override { return Role::Lecturer; }
    std::set<Permission> permissions() const override;
};

class HeadOfDepartment : public User {
public:
    using User::User;
    Role role() const override { return Role::HoD; }
    std::set<Permission> permissions() const override;
};

class Student : public User {
public:
    using User::User;
    Role role() const override { return Role::Student; }
    std::set<Permission> permissions() const override;
};

class UserFactory {
public:
    // Builds the correct subclass from the stored role. Throws ValidationError for an unknown role.
    static std::unique_ptr<User> create(const UserRecord& record);
};

// Throws PermissionError unless the actor holds the permission. Called by every service method.
void requirePermission(const User& actor, Permission permission);

}  // namespace lms
