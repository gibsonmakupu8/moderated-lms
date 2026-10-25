#include "lms/CourseworkService.hpp"

#include <cmath>

#include "lms/Validation.hpp"

namespace lms {

CourseworkService::CourseworkService(ICourseRepository& courses, IEnrolmentRepository& enrolments, IAssignmentRepository& assignments,
                                     IExtensionRepository& extensions, ISubmissionRepository& submissions, IContentStore& content,
                                     const ILatePenaltyStrategy& penalty, const IClock& clock, ITransactionRunner& tx)
    : courses_(courses), enrolments_(enrolments), assignments_(assignments), extensions_(extensions),
      submissions_(submissions), content_(content), penalty_(penalty), clock_(clock), tx_(tx) {}

Assignment CourseworkService::requireAssignment(Id assignmentId) const {
    auto assignment = assignments_.findById(assignmentId);
    if (!assignment) throw NotFoundError("Assignment #" + std::to_string(assignmentId) + " does not exist.");
    return *assignment;
}

Id CourseworkService::createAssignment(const User& lecturer, Id courseId, const std::string& title, const std::string& description,
                                       const std::string& deadline, double maxMark) {
    requirePermission(lecturer, Permission::CreateAssignment);
    if (!courses_.findById(courseId)) throw NotFoundError("Course #" + std::to_string(courseId) + " does not exist.");
    if (!courses_.isLecturerAssigned(courseId, lecturer.id())) throw PermissionError("You are not assigned to that course.");

    Assignment assignment;
    assignment.courseId = courseId;
    assignment.title = requireText(title, "Title", 100);
    assignment.description = trim(description);
    if (assignment.description.size() > 2000) throw ValidationError("Description must be at most 2000 characters.");
    const long long deadlineSeconds = parseTimestamp(deadline);
    if (deadlineSeconds <= clock_.nowSeconds()) throw ValidationError("The deadline must be in the future.");
    assignment.deadline = formatTimestamp(deadlineSeconds);
    if (!std::isfinite(maxMark) || maxMark <= 0 || maxMark > 1000) throw ValidationError("Maximum mark must be greater than 0 and at most 1000.");
    assignment.maxMark = std::round(maxMark * 100.0) / 100.0;
    assignment.createdBy = lecturer.id();
    assignment.createdAt = clock_.nowString();
    return assignments_.insert(assignment);
}

std::string CourseworkService::effectiveDeadline(const Assignment& assignment, Id studentId) const {
    auto extension = extensions_.findLatest(assignment.id, studentId);
    if (extension && parseTimestamp(extension->newDeadline) > parseTimestamp(assignment.deadline)) return extension->newDeadline;
    return assignment.deadline;
}

Id CourseworkService::grantExtension(const User& lecturer, Id assignmentId, Id studentId, const std::string& newDeadline, const std::string& reason) {
    requirePermission(lecturer, Permission::GrantExtension);
    Assignment assignment = requireAssignment(assignmentId);
    if (!courses_.isLecturerAssigned(assignment.courseId, lecturer.id())) throw PermissionError("You are not assigned to that course.");
    if (!enrolments_.isEnrolled(assignment.courseId, studentId)) throw ValidationError("That student is not enrolled in the course.");

    const long long requested = parseTimestamp(newDeadline);
    if (requested <= parseTimestamp(effectiveDeadline(assignment, studentId))) {
        throw ValidationError("The extended deadline must be later than the student's current deadline.");
    }
    Extension extension;
    extension.assignmentId = assignmentId;
    extension.studentId = studentId;
    extension.newDeadline = formatTimestamp(requested);
    extension.reason = requireText(reason, "Reason", 500);
    extension.grantedBy = lecturer.id();
    extension.grantedAt = clock_.nowString();
    return extensions_.insert(extension);
}

std::vector<Course> CourseworkService::coursesFor(const User& user) const {
    switch (user.role()) {
        case Role::Student: return courses_.findByStudent(user.id());
        case Role::Lecturer: return courses_.findByLecturer(user.id());
        case Role::HoD: return user.departmentId() ? courses_.findByDepartment(*user.departmentId()) : std::vector<Course>{};
        case Role::Admin: break;
    }
    throw PermissionError("Your role does not have a course list.");
}

std::vector<AssignmentView> CourseworkService::assignmentsFor(const User& user, const std::string& keyword) const {
    requirePermission(user, Permission::ViewAssignments);
    std::vector<AssignmentView> views;
    const std::string needle = trim(keyword);
    auto addAll = [&](const std::vector<Assignment>& list, const std::string& courseCode) {
        for (const auto& a : list) {
            if (!needle.empty() && !containsIgnoreCase(a.title, needle) && !containsIgnoreCase(courseCode, needle)) continue;
            AssignmentView view{a, courseCode, std::nullopt};
            if (user.role() == Role::Student) {
                auto latest = submissions_.findLatest(a.id, user.id());
                if (latest) view.submissionStatus = latest->status;
            }
            views.push_back(view);
        }
    };
    if (user.role() == Role::Student) {
        for (const auto& course : courses_.findByStudent(user.id())) {
            addAll(assignments_.findByCourse(course.id), course.code);
        }
    } else {
        for (const auto& course : coursesFor(user)) addAll(assignments_.findByCourse(course.id), course.code);
    }
    return views;
}

SubmissionReceipt CourseworkService::submit(const User& student, Id assignmentId, const std::string& sourceFile) {
    requirePermission(student, Permission::SubmitWork);
    Assignment assignment = requireAssignment(assignmentId);
    if (!enrolments_.isEnrolled(assignment.courseId, student.id())) throw PermissionError("You are not enrolled in this course.");

    const std::string now = clock_.nowString();
    const std::string deadline = effectiveDeadline(assignment, student.id());
    const int daysLate = penalty_.daysLate(deadline, now);
    const auto previous = submissions_.findLatest(assignmentId, student.id());

    // Resubmission is only allowed before the (extended) deadline, and only while the work is unmarked.
    if (previous) {
        if (previous->status != SubmissionStatus::Submitted) {
            throw BusinessRuleError("Your work has already been " + std::string(previous->status == SubmissionStatus::Late ? "submitted late" : "marked") +
                                    " and can no longer be replaced.");
        }
        if (daysLate > 0) throw BusinessRuleError("The deadline has passed, so your earlier submission can no longer be replaced.");
    }

    SubmissionReceipt receipt;
    receipt.attemptNo = previous ? previous->attemptNo + 1 : 1;
    receipt.status = daysLate > 0 ? SubmissionStatus::Late : SubmissionStatus::Submitted;
    receipt.daysLate = daysLate;
    receipt.penaltyPercent = penalty_.penaltyPercent(daysLate);
    receipt.submittedAt = now;
    receipt.effectiveDeadline = deadline;

    const std::string stored = content_.store(sourceFile, student.id(), assignmentId, receipt.attemptNo);
    try {
        tx_.runInTransaction([&] {
            Submission submission;
            submission.assignmentId = assignmentId;
            submission.studentId = student.id();
            submission.submittedAt = now;
            submission.status = receipt.status;
            submission.contentPath = stored;
            submission.attemptNo = receipt.attemptNo;
            submission.daysLate = daysLate;
            receipt.submissionId = submissions_.insert(submission);
        });
    } catch (...) {
        content_.discard(stored);  // do not leave an orphan file if the database write failed
        throw;
    }
    return receipt;
}

std::vector<Submission> CourseworkService::history(const User& student, Id assignmentId) const {
    requirePermission(student, Permission::SubmitWork);
    return submissions_.historyOf(assignmentId, student.id());
}

}  // namespace lms
