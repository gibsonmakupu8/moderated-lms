#include "lms/AdminService.hpp"

#include "lms/Validation.hpp"

namespace lms {

AdminService::AdminService(IUserRepository& users, IDepartmentRepository& departments, ICourseRepository& courses,
                           IEnrolmentRepository& enrolments, const IPasswordHasher& hasher)
    : users_(users), departments_(departments), courses_(courses), enrolments_(enrolments), hasher_(hasher) {}

UserRecord AdminService::requireUser(Id id, std::optional<Role> role) const {
    auto user = users_.findById(id);
    if (!user) throw NotFoundError("User #" + std::to_string(id) + " does not exist.");
    if (role && user->role != *role) throw ValidationError("User '" + user->username + "' is not a " + displayName(*role) + ".");
    return *user;
}

Course AdminService::requireCourse(Id id) const {
    auto course = courses_.findById(id);
    if (!course) throw NotFoundError("Course #" + std::to_string(id) + " does not exist.");
    return *course;
}

void AdminService::requireDepartment(Id id) const {
    if (!departments_.findById(id)) throw NotFoundError("Department #" + std::to_string(id) + " does not exist.");
}

Id AdminService::bootstrapAdmin(const std::string& username, const std::string& fullName, const std::string& password) {
    if (!needsBootstrap()) throw PermissionError("The system has already been set up.");
    UserRecord admin;
    admin.username = requireUsername(username);
    admin.fullName = requireText(fullName, "Full name", 100);
    admin.role = Role::Admin;
    requirePasswordPolicy(password);
    admin.passwordHash = hasher_.hash(password);
    return users_.insert(admin);
}

Id AdminService::createUser(const User& actor, const NewUser& in) {
    requirePermission(actor, Permission::ManageUsers);
    UserRecord record;
    record.username = requireUsername(in.username);
    record.fullName = requireText(in.fullName, "Full name", 100);
    record.role = in.role;
    record.departmentId = in.departmentId;
    if (in.role == Role::HoD && !in.departmentId) throw ValidationError("A Head of Department must belong to a department.");
    if (in.departmentId) requireDepartment(*in.departmentId);
    requirePasswordPolicy(in.password);
    if (users_.findByUsername(record.username)) throw ValidationError("The username '" + record.username + "' is already taken.");
    record.passwordHash = hasher_.hash(in.password);
    return users_.insert(record);
}

void AdminService::updateUser(const User& actor, Id userId, const std::string& fullName, std::optional<Id> departmentId) {
    requirePermission(actor, Permission::ManageUsers);
    UserRecord record = requireUser(userId);
    record.fullName = requireText(fullName, "Full name", 100);
    if (record.role == Role::HoD && !departmentId) throw ValidationError("A Head of Department must belong to a department.");
    if (departmentId) requireDepartment(*departmentId);
    record.departmentId = departmentId;
    users_.update(record);
}

void AdminService::setActive(const User& actor, Id userId, bool active) {
    requirePermission(actor, Permission::ManageUsers);
    UserRecord record = requireUser(userId);
    if (!active) {
        if (userId == actor.id()) throw BusinessRuleError("You cannot deactivate your own account.");
        if (record.role == Role::Admin) {
            int otherActive = 0;
            for (const auto& a : users_.findByRole(Role::Admin)) {
                if (a.id != userId && a.active) ++otherActive;
            }
            if (otherActive == 0) throw BusinessRuleError("At least one active administrator must remain.");
        }
    }
    record.active = active;
    users_.update(record);
}

void AdminService::resetPassword(const User& actor, Id userId, const std::string& newPassword) {
    requirePermission(actor, Permission::ManageUsers);
    UserRecord record = requireUser(userId);
    requirePasswordPolicy(newPassword);
    record.passwordHash = hasher_.hash(newPassword);
    users_.update(record);
}

std::vector<UserRecord> AdminService::listUsers(const User& actor, std::optional<Role> role) const {
    requirePermission(actor, Permission::ManageUsers);
    return role ? users_.findByRole(*role) : users_.findAll();
}

Id AdminService::createDepartment(const User& actor, const std::string& name) {
    requirePermission(actor, Permission::ManageStructure);
    Department department;
    department.name = requireText(name, "Department name", 100);
    if (departments_.findByName(department.name)) throw ValidationError("A department called '" + department.name + "' already exists.");
    return departments_.insert(department);
}

void AdminService::renameDepartment(const User& actor, Id departmentId, const std::string& name) {
    requirePermission(actor, Permission::ManageStructure);
    auto department = departments_.findById(departmentId);
    if (!department) throw NotFoundError("Department #" + std::to_string(departmentId) + " does not exist.");
    std::string clean = requireText(name, "Department name", 100);
    auto existing = departments_.findByName(clean);
    if (existing && existing->id != departmentId) throw ValidationError("A department called '" + clean + "' already exists.");
    department->name = clean;
    departments_.update(*department);
}

std::vector<Department> AdminService::listDepartments(const User& actor) const {
    requirePermission(actor, Permission::ManageStructure);
    return departments_.findAll();
}

Id AdminService::createCourse(const User& actor, const std::string& code, const std::string& title, Id departmentId) {
    requirePermission(actor, Permission::ManageStructure);
    Course course;
    course.code = requireText(code, "Course code", 20);
    course.title = requireText(title, "Course title", 100);
    course.departmentId = departmentId;
    requireDepartment(departmentId);
    if (courses_.findByCode(course.code)) throw ValidationError("A course with code '" + course.code + "' already exists.");
    return courses_.insert(course);
}

void AdminService::updateCourse(const User& actor, Id courseId, const std::string& code, const std::string& title, Id departmentId) {
    requirePermission(actor, Permission::ManageStructure);
    Course course = requireCourse(courseId);
    course.code = requireText(code, "Course code", 20);
    course.title = requireText(title, "Course title", 100);
    course.departmentId = departmentId;
    requireDepartment(departmentId);
    auto existing = courses_.findByCode(course.code);
    if (existing && existing->id != courseId) throw ValidationError("A course with code '" + course.code + "' already exists.");
    courses_.update(course);
}

std::vector<Course> AdminService::listCourses(const User& actor) const {
    requirePermission(actor, Permission::ManageStructure);
    return courses_.findAll();
}

void AdminService::assignLecturer(const User& actor, Id courseId, Id lecturerId) {
    requirePermission(actor, Permission::ManageStructure);
    requireCourse(courseId);
    UserRecord lecturer = requireUser(lecturerId, Role::Lecturer);
    if (!lecturer.active) throw BusinessRuleError("That lecturer's account is deactivated.");
    if (courses_.isLecturerAssigned(courseId, lecturerId)) throw ValidationError("That lecturer is already assigned to the course.");
    courses_.assignLecturer(courseId, lecturerId);
}

Id AdminService::enrolStudent(const User& actor, Id courseId, Id studentId) {
    requirePermission(actor, Permission::ManageStructure);
    requireCourse(courseId);
    UserRecord student = requireUser(studentId, Role::Student);
    if (!student.active) throw BusinessRuleError("That student's account is deactivated.");
    if (enrolments_.isEnrolled(courseId, studentId)) throw ValidationError("That student is already enrolled in the course.");
    Enrolment enrolment;
    enrolment.courseId = courseId;
    enrolment.studentId = studentId;
    return enrolments_.insert(enrolment);
}

std::vector<UserRecord> AdminService::studentsOf(const User& actor, Id courseId) const {
    requirePermission(actor, Permission::ManageStructure);
    return enrolments_.studentsOf(courseId);
}

}  // namespace lms
