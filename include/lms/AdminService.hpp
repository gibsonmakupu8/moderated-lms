#pragma once

#include <vector>

#include "lms/PasswordHasher.hpp"
#include "lms/Repositories.hpp"
#include "lms/User.hpp"

namespace lms {

struct NewUser {
    std::string username;
    std::string fullName;
    Role role = Role::Student;
    std::string password;
    std::optional<Id> departmentId;
};

// Administration use cases (FR-01, FR-04, FR-05). Every method checks the actor's permission.
class AdminService {
public:
    AdminService(IUserRepository& users, IDepartmentRepository& departments, ICourseRepository& courses,
                 IEnrolmentRepository& enrolments, const IPasswordHasher& hasher);

    // First-run set-up: creates the first administrator, only while no account exists.
    Id bootstrapAdmin(const std::string& username, const std::string& fullName, const std::string& password);
    bool needsBootstrap() const { return users_.count() == 0; }

    Id createUser(const User& actor, const NewUser& newUser);
    void updateUser(const User& actor, Id userId, const std::string& fullName, std::optional<Id> departmentId);
    void setActive(const User& actor, Id userId, bool active);
    void resetPassword(const User& actor, Id userId, const std::string& newPassword);
    std::vector<UserRecord> listUsers(const User& actor, std::optional<Role> role = std::nullopt) const;

    Id createDepartment(const User& actor, const std::string& name);
    void renameDepartment(const User& actor, Id departmentId, const std::string& name);
    std::vector<Department> listDepartments(const User& actor) const;

    Id createCourse(const User& actor, const std::string& code, const std::string& title, Id departmentId);
    void updateCourse(const User& actor, Id courseId, const std::string& code, const std::string& title, Id departmentId);
    std::vector<Course> listCourses(const User& actor) const;

    void assignLecturer(const User& actor, Id courseId, Id lecturerId);
    Id enrolStudent(const User& actor, Id courseId, Id studentId);
    std::vector<UserRecord> studentsOf(const User& actor, Id courseId) const;

private:
    UserRecord requireUser(Id id, std::optional<Role> role = std::nullopt) const;
    Course requireCourse(Id id) const;
    void requireDepartment(Id id) const;

    IUserRepository& users_;
    IDepartmentRepository& departments_;
    ICourseRepository& courses_;
    IEnrolmentRepository& enrolments_;
    const IPasswordHasher& hasher_;
};

}  // namespace lms
