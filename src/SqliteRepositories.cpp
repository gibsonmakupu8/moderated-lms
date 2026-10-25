#include "lms/SqliteRepositories.hpp"

namespace lms {
namespace {

// ---- row mappers (column order matches the SELECT lists below) -------------------------------
const char* const kUserCols = "user_id, username, full_name, role, password_hash, department_id, active";
UserRecord mapUser(const Statement& s) {
    UserRecord u;
    u.id = s.integer(0);
    u.username = s.text(1);
    u.fullName = s.text(2);
    u.role = roleFromString(s.text(3));
    u.passwordHash = s.text(4);
    u.departmentId = s.optionalInteger(5);
    u.active = s.integer(6) != 0;
    return u;
}

const char* const kCourseCols = "c.course_id, c.course_code, c.title, c.department_id";
Course mapCourse(const Statement& s) {
    Course c;
    c.id = s.integer(0);
    c.code = s.text(1);
    c.title = s.text(2);
    c.departmentId = s.integer(3);
    return c;
}

const char* const kAssignmentCols = "a.assignment_id, a.course_id, a.title, a.description, a.deadline, a.max_mark, a.created_by, a.created_at";
Assignment mapAssignment(const Statement& s) {
    Assignment a;
    a.id = s.integer(0);
    a.courseId = s.integer(1);
    a.title = s.text(2);
    a.description = s.text(3);
    a.deadline = s.text(4);
    a.maxMark = s.real(5);
    a.createdBy = s.integer(6);
    a.createdAt = s.text(7);
    return a;
}

const char* const kExtensionCols = "extension_id, assignment_id, student_id, new_deadline, reason, granted_by, granted_at";
Extension mapExtension(const Statement& s) {
    Extension e;
    e.id = s.integer(0);
    e.assignmentId = s.integer(1);
    e.studentId = s.integer(2);
    e.newDeadline = s.text(3);
    e.reason = s.text(4);
    e.grantedBy = s.integer(5);
    e.grantedAt = s.text(6);
    return e;
}

const char* const kSubmissionCols = "s.submission_id, s.assignment_id, s.student_id, s.submitted_at, s.status, s.content_path, s.attempt_no, s.days_late";
Submission mapSubmission(const Statement& s) {
    Submission x;
    x.id = s.integer(0);
    x.assignmentId = s.integer(1);
    x.studentId = s.integer(2);
    x.submittedAt = s.text(3);
    x.status = submissionStatusFromString(s.text(4));
    x.contentPath = s.text(5);
    x.attemptNo = static_cast<int>(s.integer(6));
    x.daysLate = static_cast<int>(s.integer(7));
    return x;
}

// Restricts a submissions query (alias s) to each student's latest attempt.
const char* const kLatestOnly =
    " s.attempt_no = (SELECT MAX(attempt_no) FROM submissions WHERE assignment_id = s.assignment_id AND student_id = s.student_id)";

const char* const kMarkCols =
    "mark_id, submission_id, raw_mark, penalty_percent, final_mark, feedback, marked_by, marked_at, moderation_status, "
    "moderated_by, moderated_at, moderation_comment, released_by, released_at";
Mark mapMark(const Statement& s) {
    Mark m;
    m.id = s.integer(0);
    m.submissionId = s.integer(1);
    m.rawMark = s.real(2);
    m.penaltyPercent = s.real(3);
    m.finalMark = s.real(4);
    m.feedback = s.text(5);
    m.markedBy = s.integer(6);
    m.markedAt = s.text(7);
    m.moderationStatus = moderationStatusFromString(s.text(8));
    m.moderatedBy = s.optionalInteger(9);
    m.moderatedAt = s.optionalText(10);
    m.moderationComment = s.optionalText(11);
    m.releasedBy = s.optionalInteger(12);
    m.releasedAt = s.optionalText(13);
    return m;
}

const char* const kAuditCols = "audit_id, mark_id, changed_by, changed_at, event_type, old_value, new_value, reason";
AuditEntry mapAudit(const Statement& s) {
    AuditEntry a;
    a.id = s.integer(0);
    a.markId = s.integer(1);
    a.changedBy = s.integer(2);
    a.changedAt = s.text(3);
    a.event = auditEventFromString(s.text(4));
    a.oldValue = s.optionalText(5);
    a.newValue = s.optionalText(6);
    a.reason = s.text(7);
    return a;
}

template <typename T, typename Mapper>
std::vector<T> collect(Statement& st, Mapper mapper) {
    std::vector<T> out;
    while (st.step()) out.push_back(mapper(st));
    return out;
}

template <typename T, typename Mapper>
std::optional<T> first(Statement& st, Mapper mapper) {
    if (st.step()) return mapper(st);
    return std::nullopt;
}

}  // namespace

// ================================ users ========================================================
Id SqliteUserRepository::insert(const UserRecord& u) {
    Statement st(db_, "INSERT INTO users (username, full_name, role, password_hash, department_id, active) VALUES (?1,?2,?3,?4,?5,?6)");
    st.bind(1, u.username).bind(2, u.fullName).bind(3, toString(u.role)).bind(4, u.passwordHash).bind(5, u.departmentId).bind(6, u.active ? 1 : 0);
    st.run();
    return db_.lastInsertId();
}
std::optional<UserRecord> SqliteUserRepository::findById(Id id) const {
    Statement st(db_, std::string("SELECT ") + kUserCols + " FROM users WHERE user_id = ?1");
    st.bind(1, id);
    return first<UserRecord>(st, mapUser);
}
std::vector<UserRecord> SqliteUserRepository::findAll() const {
    Statement st(db_, std::string("SELECT ") + kUserCols + " FROM users ORDER BY username");
    return collect<UserRecord>(st, mapUser);
}
void SqliteUserRepository::update(const UserRecord& u) {
    Statement st(db_, "UPDATE users SET full_name=?2, role=?3, password_hash=?4, department_id=?5, active=?6 WHERE user_id=?1");
    st.bind(1, u.id).bind(2, u.fullName).bind(3, toString(u.role)).bind(4, u.passwordHash).bind(5, u.departmentId).bind(6, u.active ? 1 : 0);
    st.run();
}
std::optional<UserRecord> SqliteUserRepository::findByUsername(const std::string& username) const {
    Statement st(db_, std::string("SELECT ") + kUserCols + " FROM users WHERE username = ?1");
    st.bind(1, username);
    return first<UserRecord>(st, mapUser);
}
std::vector<UserRecord> SqliteUserRepository::findByRole(Role role) const {
    Statement st(db_, std::string("SELECT ") + kUserCols + " FROM users WHERE role = ?1 ORDER BY full_name");
    st.bind(1, toString(role));
    return collect<UserRecord>(st, mapUser);
}
long long SqliteUserRepository::count() const {
    Statement st(db_, "SELECT COUNT(*) FROM users");
    st.step();
    return st.integer(0);
}

// ============================== departments ====================================================
namespace {
Department mapDepartment(const Statement& s) { return Department{s.integer(0), s.text(1)}; }
}
Id SqliteDepartmentRepository::insert(const Department& d) {
    Statement st(db_, "INSERT INTO departments (name) VALUES (?1)");
    st.bind(1, d.name);
    st.run();
    return db_.lastInsertId();
}
std::optional<Department> SqliteDepartmentRepository::findById(Id id) const {
    Statement st(db_, "SELECT department_id, name FROM departments WHERE department_id = ?1");
    st.bind(1, id);
    return first<Department>(st, mapDepartment);
}
std::vector<Department> SqliteDepartmentRepository::findAll() const {
    Statement st(db_, "SELECT department_id, name FROM departments ORDER BY name");
    return collect<Department>(st, mapDepartment);
}
void SqliteDepartmentRepository::update(const Department& d) {
    Statement st(db_, "UPDATE departments SET name=?2 WHERE department_id=?1");
    st.bind(1, d.id).bind(2, d.name);
    st.run();
}
std::optional<Department> SqliteDepartmentRepository::findByName(const std::string& name) const {
    Statement st(db_, "SELECT department_id, name FROM departments WHERE name = ?1");
    st.bind(1, name);
    return first<Department>(st, mapDepartment);
}

// ================================ courses ======================================================
Id SqliteCourseRepository::insert(const Course& c) {
    Statement st(db_, "INSERT INTO courses (course_code, title, department_id) VALUES (?1,?2,?3)");
    st.bind(1, c.code).bind(2, c.title).bind(3, c.departmentId);
    st.run();
    return db_.lastInsertId();
}
std::optional<Course> SqliteCourseRepository::findById(Id id) const {
    Statement st(db_, std::string("SELECT ") + kCourseCols + " FROM courses c WHERE c.course_id = ?1");
    st.bind(1, id);
    return first<Course>(st, mapCourse);
}
std::vector<Course> SqliteCourseRepository::findAll() const {
    Statement st(db_, std::string("SELECT ") + kCourseCols + " FROM courses c ORDER BY c.course_code");
    return collect<Course>(st, mapCourse);
}
void SqliteCourseRepository::update(const Course& c) {
    Statement st(db_, "UPDATE courses SET course_code=?2, title=?3, department_id=?4 WHERE course_id=?1");
    st.bind(1, c.id).bind(2, c.code).bind(3, c.title).bind(4, c.departmentId);
    st.run();
}
std::optional<Course> SqliteCourseRepository::findByCode(const std::string& code) const {
    Statement st(db_, std::string("SELECT ") + kCourseCols + " FROM courses c WHERE c.course_code = ?1");
    st.bind(1, code);
    return first<Course>(st, mapCourse);
}
std::vector<Course> SqliteCourseRepository::findByDepartment(Id departmentId) const {
    Statement st(db_, std::string("SELECT ") + kCourseCols + " FROM courses c WHERE c.department_id = ?1 ORDER BY c.course_code");
    st.bind(1, departmentId);
    return collect<Course>(st, mapCourse);
}
std::vector<Course> SqliteCourseRepository::findByLecturer(Id lecturerId) const {
    Statement st(db_, std::string("SELECT ") + kCourseCols +
                          " FROM courses c JOIN course_lecturers cl ON cl.course_id = c.course_id WHERE cl.lecturer_id = ?1 ORDER BY c.course_code");
    st.bind(1, lecturerId);
    return collect<Course>(st, mapCourse);
}
std::vector<Course> SqliteCourseRepository::findByStudent(Id studentId) const {
    Statement st(db_, std::string("SELECT ") + kCourseCols +
                          " FROM courses c JOIN enrolments e ON e.course_id = c.course_id WHERE e.student_id = ?1 ORDER BY c.course_code");
    st.bind(1, studentId);
    return collect<Course>(st, mapCourse);
}
void SqliteCourseRepository::assignLecturer(Id courseId, Id lecturerId) {
    Statement st(db_, "INSERT INTO course_lecturers (course_id, lecturer_id) VALUES (?1,?2)");
    st.bind(1, courseId).bind(2, lecturerId);
    st.run();
}
bool SqliteCourseRepository::isLecturerAssigned(Id courseId, Id lecturerId) const {
    Statement st(db_, "SELECT 1 FROM course_lecturers WHERE course_id = ?1 AND lecturer_id = ?2");
    st.bind(1, courseId).bind(2, lecturerId);
    return st.step();
}

// =============================== enrolments ====================================================
namespace {
Enrolment mapEnrolment(const Statement& s) { return Enrolment{s.integer(0), s.integer(1), s.integer(2)}; }
}
Id SqliteEnrolmentRepository::insert(const Enrolment& e) {
    Statement st(db_, "INSERT INTO enrolments (course_id, student_id) VALUES (?1,?2)");
    st.bind(1, e.courseId).bind(2, e.studentId);
    st.run();
    return db_.lastInsertId();
}
std::optional<Enrolment> SqliteEnrolmentRepository::findById(Id id) const {
    Statement st(db_, "SELECT enrolment_id, course_id, student_id FROM enrolments WHERE enrolment_id = ?1");
    st.bind(1, id);
    return first<Enrolment>(st, mapEnrolment);
}
std::vector<Enrolment> SqliteEnrolmentRepository::findAll() const {
    Statement st(db_, "SELECT enrolment_id, course_id, student_id FROM enrolments ORDER BY enrolment_id");
    return collect<Enrolment>(st, mapEnrolment);
}
bool SqliteEnrolmentRepository::isEnrolled(Id courseId, Id studentId) const {
    Statement st(db_, "SELECT 1 FROM enrolments WHERE course_id = ?1 AND student_id = ?2");
    st.bind(1, courseId).bind(2, studentId);
    return st.step();
}
std::vector<UserRecord> SqliteEnrolmentRepository::studentsOf(Id courseId) const {
    Statement st(db_, "SELECT u.user_id, u.username, u.full_name, u.role, u.password_hash, u.department_id, u.active "
                      "FROM users u JOIN enrolments e ON e.student_id = u.user_id WHERE e.course_id = ?1 ORDER BY u.full_name");
    st.bind(1, courseId);
    return collect<UserRecord>(st, mapUser);
}

// =============================== assignments ===================================================
Id SqliteAssignmentRepository::insert(const Assignment& a) {
    Statement st(db_, "INSERT INTO assignments (course_id, title, description, deadline, max_mark, created_by, created_at) VALUES (?1,?2,?3,?4,?5,?6,?7)");
    st.bind(1, a.courseId).bind(2, a.title).bind(3, a.description).bind(4, a.deadline).bind(5, a.maxMark).bind(6, a.createdBy).bind(7, a.createdAt);
    st.run();
    return db_.lastInsertId();
}
std::optional<Assignment> SqliteAssignmentRepository::findById(Id id) const {
    Statement st(db_, std::string("SELECT ") + kAssignmentCols + " FROM assignments a WHERE a.assignment_id = ?1");
    st.bind(1, id);
    return first<Assignment>(st, mapAssignment);
}
std::vector<Assignment> SqliteAssignmentRepository::findAll() const {
    Statement st(db_, std::string("SELECT ") + kAssignmentCols + " FROM assignments a ORDER BY a.deadline");
    return collect<Assignment>(st, mapAssignment);
}
std::vector<Assignment> SqliteAssignmentRepository::findByCourse(Id courseId) const {
    Statement st(db_, std::string("SELECT ") + kAssignmentCols + " FROM assignments a WHERE a.course_id = ?1 ORDER BY a.deadline");
    st.bind(1, courseId);
    return collect<Assignment>(st, mapAssignment);
}
std::vector<Assignment> SqliteAssignmentRepository::findByStudent(Id studentId) const {
    Statement st(db_, std::string("SELECT ") + kAssignmentCols +
                          " FROM assignments a JOIN enrolments e ON e.course_id = a.course_id WHERE e.student_id = ?1 ORDER BY a.deadline");
    st.bind(1, studentId);
    return collect<Assignment>(st, mapAssignment);
}

// =============================== extensions ====================================================
Id SqliteExtensionRepository::insert(const Extension& e) {
    Statement st(db_, "INSERT INTO extensions (assignment_id, student_id, new_deadline, reason, granted_by, granted_at) VALUES (?1,?2,?3,?4,?5,?6)");
    st.bind(1, e.assignmentId).bind(2, e.studentId).bind(3, e.newDeadline).bind(4, e.reason).bind(5, e.grantedBy).bind(6, e.grantedAt);
    st.run();
    return db_.lastInsertId();
}
std::optional<Extension> SqliteExtensionRepository::findById(Id id) const {
    Statement st(db_, std::string("SELECT ") + kExtensionCols + " FROM extensions WHERE extension_id = ?1");
    st.bind(1, id);
    return first<Extension>(st, mapExtension);
}
std::vector<Extension> SqliteExtensionRepository::findAll() const {
    Statement st(db_, std::string("SELECT ") + kExtensionCols + " FROM extensions ORDER BY extension_id");
    return collect<Extension>(st, mapExtension);
}
std::optional<Extension> SqliteExtensionRepository::findLatest(Id assignmentId, Id studentId) const {
    Statement st(db_, std::string("SELECT ") + kExtensionCols +
                          " FROM extensions WHERE assignment_id = ?1 AND student_id = ?2 ORDER BY new_deadline DESC, extension_id DESC LIMIT 1");
    st.bind(1, assignmentId).bind(2, studentId);
    return first<Extension>(st, mapExtension);
}

// =============================== submissions ===================================================
Id SqliteSubmissionRepository::insert(const Submission& s) {
    Statement st(db_, "INSERT INTO submissions (assignment_id, student_id, submitted_at, status, content_path, attempt_no, days_late) VALUES (?1,?2,?3,?4,?5,?6,?7)");
    st.bind(1, s.assignmentId).bind(2, s.studentId).bind(3, s.submittedAt).bind(4, toString(s.status)).bind(5, s.contentPath).bind(6, s.attemptNo).bind(7, s.daysLate);
    st.run();
    return db_.lastInsertId();
}
std::optional<Submission> SqliteSubmissionRepository::findById(Id id) const {
    Statement st(db_, std::string("SELECT ") + kSubmissionCols + " FROM submissions s WHERE s.submission_id = ?1");
    st.bind(1, id);
    return first<Submission>(st, mapSubmission);
}
std::vector<Submission> SqliteSubmissionRepository::findAll() const {
    Statement st(db_, std::string("SELECT ") + kSubmissionCols + " FROM submissions s ORDER BY s.submission_id");
    return collect<Submission>(st, mapSubmission);
}
void SqliteSubmissionRepository::update(const Submission& s) {
    Statement st(db_, "UPDATE submissions SET status=?2, content_path=?3, days_late=?4 WHERE submission_id=?1");
    st.bind(1, s.id).bind(2, toString(s.status)).bind(3, s.contentPath).bind(4, s.daysLate);
    st.run();
}
std::optional<Submission> SqliteSubmissionRepository::findLatest(Id assignmentId, Id studentId) const {
    Statement st(db_, std::string("SELECT ") + kSubmissionCols +
                          " FROM submissions s WHERE s.assignment_id = ?1 AND s.student_id = ?2 ORDER BY s.attempt_no DESC LIMIT 1");
    st.bind(1, assignmentId).bind(2, studentId);
    return first<Submission>(st, mapSubmission);
}
std::vector<Submission> SqliteSubmissionRepository::historyOf(Id assignmentId, Id studentId) const {
    Statement st(db_, std::string("SELECT ") + kSubmissionCols +
                          " FROM submissions s WHERE s.assignment_id = ?1 AND s.student_id = ?2 ORDER BY s.attempt_no");
    st.bind(1, assignmentId).bind(2, studentId);
    return collect<Submission>(st, mapSubmission);
}
std::vector<Submission> SqliteSubmissionRepository::latestByCourse(Id courseId, std::optional<SubmissionStatus> status) const {
    Statement st(db_, std::string("SELECT ") + kSubmissionCols +
                          " FROM submissions s JOIN assignments a ON a.assignment_id = s.assignment_id WHERE a.course_id = ?1 AND" +
                          kLatestOnly + (status ? " AND s.status = ?2" : "") + " ORDER BY s.submitted_at");
    st.bind(1, courseId);
    if (status) st.bind(2, toString(*status));
    return collect<Submission>(st, mapSubmission);
}
std::vector<Submission> SqliteSubmissionRepository::latestByDepartment(Id departmentId, std::optional<SubmissionStatus> status) const {
    Statement st(db_, std::string("SELECT ") + kSubmissionCols +
                          " FROM submissions s JOIN assignments a ON a.assignment_id = s.assignment_id "
                          "JOIN courses c ON c.course_id = a.course_id WHERE c.department_id = ?1 AND" +
                          kLatestOnly + (status ? " AND s.status = ?2" : "") + " ORDER BY s.submitted_at");
    st.bind(1, departmentId);
    if (status) st.bind(2, toString(*status));
    return collect<Submission>(st, mapSubmission);
}
std::vector<Submission> SqliteSubmissionRepository::latestByStudent(Id studentId) const {
    Statement st(db_, std::string("SELECT ") + kSubmissionCols + " FROM submissions s WHERE s.student_id = ?1 AND" + kLatestOnly +
                          " ORDER BY s.submitted_at");
    st.bind(1, studentId);
    return collect<Submission>(st, mapSubmission);
}

// ================================= marks =======================================================
Id SqliteMarkRepository::insert(const Mark& m) {
    Statement st(db_, "INSERT INTO marks (submission_id, raw_mark, penalty_percent, final_mark, feedback, marked_by, marked_at, moderation_status) VALUES (?1,?2,?3,?4,?5,?6,?7,?8)");
    st.bind(1, m.submissionId).bind(2, m.rawMark).bind(3, m.penaltyPercent).bind(4, m.finalMark).bind(5, m.feedback).bind(6, m.markedBy).bind(7, m.markedAt).bind(8, toString(m.moderationStatus));
    st.run();
    return db_.lastInsertId();
}
std::optional<Mark> SqliteMarkRepository::findById(Id id) const {
    Statement st(db_, std::string("SELECT ") + kMarkCols + " FROM marks WHERE mark_id = ?1");
    st.bind(1, id);
    return first<Mark>(st, mapMark);
}
std::vector<Mark> SqliteMarkRepository::findAll() const {
    Statement st(db_, std::string("SELECT ") + kMarkCols + " FROM marks ORDER BY mark_id");
    return collect<Mark>(st, mapMark);
}
void SqliteMarkRepository::update(const Mark& m) {
    Statement st(db_,
                 "UPDATE marks SET raw_mark=?2, penalty_percent=?3, final_mark=?4, feedback=?5, marked_by=?6, marked_at=?7, "
                 "moderation_status=?8, moderated_by=?9, moderated_at=?10, moderation_comment=?11, released_by=?12, released_at=?13 WHERE mark_id=?1");
    st.bind(1, m.id).bind(2, m.rawMark).bind(3, m.penaltyPercent).bind(4, m.finalMark).bind(5, m.feedback).bind(6, m.markedBy).bind(7, m.markedAt)
        .bind(8, toString(m.moderationStatus)).bind(9, m.moderatedBy).bind(10, m.moderatedAt).bind(11, m.moderationComment)
        .bind(12, m.releasedBy).bind(13, m.releasedAt);
    st.run();
}
std::optional<Mark> SqliteMarkRepository::findBySubmission(Id submissionId) const {
    Statement st(db_, std::string("SELECT ") + kMarkCols + " FROM marks WHERE submission_id = ?1");
    st.bind(1, submissionId);
    return first<Mark>(st, mapMark);
}

// ================================= audit =======================================================
Id SqliteAuditRepository::insert(const AuditEntry& a) {
    Statement st(db_, "INSERT INTO audit_entries (mark_id, changed_by, changed_at, event_type, old_value, new_value, reason) VALUES (?1,?2,?3,?4,?5,?6,?7)");
    st.bind(1, a.markId).bind(2, a.changedBy).bind(3, a.changedAt).bind(4, toString(a.event)).bind(5, a.oldValue).bind(6, a.newValue).bind(7, a.reason);
    st.run();
    return db_.lastInsertId();
}
std::optional<AuditEntry> SqliteAuditRepository::findById(Id id) const {
    Statement st(db_, std::string("SELECT ") + kAuditCols + " FROM audit_entries WHERE audit_id = ?1");
    st.bind(1, id);
    return first<AuditEntry>(st, mapAudit);
}
std::vector<AuditEntry> SqliteAuditRepository::findAll() const {
    Statement st(db_, std::string("SELECT ") + kAuditCols + " FROM audit_entries ORDER BY audit_id");
    return collect<AuditEntry>(st, mapAudit);
}
std::vector<AuditEntry> SqliteAuditRepository::findByMark(Id markId) const {
    Statement st(db_, std::string("SELECT ") + kAuditCols + " FROM audit_entries WHERE mark_id = ?1 ORDER BY audit_id");
    st.bind(1, markId);
    return collect<AuditEntry>(st, mapAudit);
}

}  // namespace lms
