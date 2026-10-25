// Repository / DAO interfaces (Repository pattern, SDD 4.2). Services depend on these abstractions,
// never on SQL (Dependency Inversion). Interfaces are small and role-specific (Interface Segregation).
#pragma once

#include <optional>
#include <vector>

#include "lms/Models.hpp"

namespace lms {

template <typename T>
class Repository {
public:
    virtual ~Repository() = default;
    virtual Id insert(const T& entity) = 0;  // returns the new id
    virtual std::optional<T> findById(Id id) const = 0;
    virtual std::vector<T> findAll() const = 0;
};

class IUserRepository : public Repository<UserRecord> {
public:
    virtual void update(const UserRecord& user) = 0;
    virtual std::optional<UserRecord> findByUsername(const std::string& username) const = 0;
    virtual std::vector<UserRecord> findByRole(Role role) const = 0;
    virtual long long count() const = 0;
};

class IDepartmentRepository : public Repository<Department> {
public:
    virtual void update(const Department& department) = 0;
    virtual std::optional<Department> findByName(const std::string& name) const = 0;
};

class ICourseRepository : public Repository<Course> {
public:
    virtual void update(const Course& course) = 0;
    virtual std::optional<Course> findByCode(const std::string& code) const = 0;
    virtual std::vector<Course> findByDepartment(Id departmentId) const = 0;
    virtual std::vector<Course> findByLecturer(Id lecturerId) const = 0;
    virtual std::vector<Course> findByStudent(Id studentId) const = 0;
    virtual void assignLecturer(Id courseId, Id lecturerId) = 0;
    virtual bool isLecturerAssigned(Id courseId, Id lecturerId) const = 0;
};

class IEnrolmentRepository : public Repository<Enrolment> {
public:
    virtual bool isEnrolled(Id courseId, Id studentId) const = 0;
    virtual std::vector<UserRecord> studentsOf(Id courseId) const = 0;
};

class IAssignmentRepository : public Repository<Assignment> {
public:
    virtual std::vector<Assignment> findByCourse(Id courseId) const = 0;
    virtual std::vector<Assignment> findByStudent(Id studentId) const = 0;  // via enrolments
};

class IExtensionRepository : public Repository<Extension> {
public:
    // Latest (furthest) extension for this student on this assignment, if any.
    virtual std::optional<Extension> findLatest(Id assignmentId, Id studentId) const = 0;
};

class ISubmissionRepository : public Repository<Submission> {
public:
    virtual void update(const Submission& submission) = 0;
    // Latest attempt of this student for this assignment.
    virtual std::optional<Submission> findLatest(Id assignmentId, Id studentId) const = 0;
    virtual std::vector<Submission> historyOf(Id assignmentId, Id studentId) const = 0;
    // The queries below only return each student's latest attempt.
    virtual std::vector<Submission> latestByCourse(Id courseId, std::optional<SubmissionStatus> status) const = 0;
    virtual std::vector<Submission> latestByDepartment(Id departmentId, std::optional<SubmissionStatus> status) const = 0;
    virtual std::vector<Submission> latestByStudent(Id studentId) const = 0;
};

class IMarkRepository : public Repository<Mark> {
public:
    virtual void update(const Mark& mark) = 0;
    virtual std::optional<Mark> findBySubmission(Id submissionId) const = 0;
};

class IAuditRepository : public Repository<AuditEntry> {
public:
    virtual std::vector<AuditEntry> findByMark(Id markId) const = 0;
};

}  // namespace lms
