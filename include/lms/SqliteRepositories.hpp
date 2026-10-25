// SQLite implementations of the repository interfaces. This is the ONLY place SQL is written.
#pragma once

#include "lms/Database.hpp"
#include "lms/Repositories.hpp"

namespace lms {

class SqliteRepositoryBase {
protected:
    explicit SqliteRepositoryBase(Database& db) : db_(db) {}
    Database& db_;
};

class SqliteUserRepository : public IUserRepository, private SqliteRepositoryBase {
public:
    explicit SqliteUserRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const UserRecord& user) override;
    std::optional<UserRecord> findById(Id id) const override;
    std::vector<UserRecord> findAll() const override;
    void update(const UserRecord& user) override;
    std::optional<UserRecord> findByUsername(const std::string& username) const override;
    std::vector<UserRecord> findByRole(Role role) const override;
    long long count() const override;
};

class SqliteDepartmentRepository : public IDepartmentRepository, private SqliteRepositoryBase {
public:
    explicit SqliteDepartmentRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const Department& department) override;
    std::optional<Department> findById(Id id) const override;
    std::vector<Department> findAll() const override;
    void update(const Department& department) override;
    std::optional<Department> findByName(const std::string& name) const override;
};

class SqliteCourseRepository : public ICourseRepository, private SqliteRepositoryBase {
public:
    explicit SqliteCourseRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const Course& course) override;
    std::optional<Course> findById(Id id) const override;
    std::vector<Course> findAll() const override;
    void update(const Course& course) override;
    std::optional<Course> findByCode(const std::string& code) const override;
    std::vector<Course> findByDepartment(Id departmentId) const override;
    std::vector<Course> findByLecturer(Id lecturerId) const override;
    std::vector<Course> findByStudent(Id studentId) const override;
    void assignLecturer(Id courseId, Id lecturerId) override;
    bool isLecturerAssigned(Id courseId, Id lecturerId) const override;
};

class SqliteEnrolmentRepository : public IEnrolmentRepository, private SqliteRepositoryBase {
public:
    explicit SqliteEnrolmentRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const Enrolment& enrolment) override;
    std::optional<Enrolment> findById(Id id) const override;
    std::vector<Enrolment> findAll() const override;
    bool isEnrolled(Id courseId, Id studentId) const override;
    std::vector<UserRecord> studentsOf(Id courseId) const override;
};

class SqliteAssignmentRepository : public IAssignmentRepository, private SqliteRepositoryBase {
public:
    explicit SqliteAssignmentRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const Assignment& assignment) override;
    std::optional<Assignment> findById(Id id) const override;
    std::vector<Assignment> findAll() const override;
    std::vector<Assignment> findByCourse(Id courseId) const override;
    std::vector<Assignment> findByStudent(Id studentId) const override;
};

class SqliteExtensionRepository : public IExtensionRepository, private SqliteRepositoryBase {
public:
    explicit SqliteExtensionRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const Extension& extension) override;
    std::optional<Extension> findById(Id id) const override;
    std::vector<Extension> findAll() const override;
    std::optional<Extension> findLatest(Id assignmentId, Id studentId) const override;
};

class SqliteSubmissionRepository : public ISubmissionRepository, private SqliteRepositoryBase {
public:
    explicit SqliteSubmissionRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const Submission& submission) override;
    std::optional<Submission> findById(Id id) const override;
    std::vector<Submission> findAll() const override;
    void update(const Submission& submission) override;
    std::optional<Submission> findLatest(Id assignmentId, Id studentId) const override;
    std::vector<Submission> historyOf(Id assignmentId, Id studentId) const override;
    std::vector<Submission> latestByCourse(Id courseId, std::optional<SubmissionStatus> status) const override;
    std::vector<Submission> latestByDepartment(Id departmentId, std::optional<SubmissionStatus> status) const override;
    std::vector<Submission> latestByStudent(Id studentId) const override;
};

class SqliteMarkRepository : public IMarkRepository, private SqliteRepositoryBase {
public:
    explicit SqliteMarkRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const Mark& mark) override;
    std::optional<Mark> findById(Id id) const override;
    std::vector<Mark> findAll() const override;
    void update(const Mark& mark) override;
    std::optional<Mark> findBySubmission(Id submissionId) const override;
};

class SqliteAuditRepository : public IAuditRepository, private SqliteRepositoryBase {
public:
    explicit SqliteAuditRepository(Database& db) : SqliteRepositoryBase(db) {}
    Id insert(const AuditEntry& entry) override;
    std::optional<AuditEntry> findById(Id id) const override;
    std::vector<AuditEntry> findAll() const override;
    std::vector<AuditEntry> findByMark(Id markId) const override;
};

}  // namespace lms
