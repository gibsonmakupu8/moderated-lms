#include "lms/User.hpp"

namespace lms {

std::string toString(Permission permission) {
    switch (permission) {
        case Permission::ManageUsers: return "manage user accounts";
        case Permission::ManageStructure: return "manage departments, courses and enrolments";
        case Permission::ViewAssignments: return "view assignments";
        case Permission::CreateAssignment: return "create assignments";
        case Permission::GrantExtension: return "grant extensions";
        case Permission::SubmitWork: return "submit work";
        case Permission::MarkSubmissions: return "mark submissions";
        case Permission::ModerateMarks: return "moderate marks";
        case Permission::ReleaseMarks: return "release marks";
        case Permission::ViewReleasedResults: return "view released results";
    }
    return "perform this action";
}

std::set<Permission> SystemAdministrator::permissions() const {
    return {Permission::ManageUsers, Permission::ManageStructure};
}

std::set<Permission> Lecturer::permissions() const {
    return {Permission::ViewAssignments, Permission::CreateAssignment, Permission::GrantExtension,
            Permission::MarkSubmissions};
}

std::set<Permission> HeadOfDepartment::permissions() const {
    return {Permission::ViewAssignments, Permission::ModerateMarks, Permission::ReleaseMarks};
}

std::set<Permission> Student::permissions() const {
    return {Permission::ViewAssignments, Permission::SubmitWork, Permission::ViewReleasedResults};
}

std::unique_ptr<User> UserFactory::create(const UserRecord& record) {
    switch (record.role) {
        case Role::Admin: return std::make_unique<SystemAdministrator>(record);
        case Role::Lecturer: return std::make_unique<Lecturer>(record);
        case Role::HoD: return std::make_unique<HeadOfDepartment>(record);
        case Role::Student: return std::make_unique<Student>(record);
    }
    throw ValidationError("Cannot create a user with an unknown role");
}

void requirePermission(const User& actor, Permission permission) {
    if (!actor.can(permission)) {
        throw PermissionError("Your role (" + actor.roleName() + ") is not permitted to " + toString(permission) + ".");
    }
}

}  // namespace lms
