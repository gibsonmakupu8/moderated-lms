#pragma once

#include <vector>

#include "lms/ContentStore.hpp"
#include "lms/Database.hpp"
#include "lms/LatePenalty.hpp"
#include "lms/Repositories.hpp"
#include "lms/TimeUtil.hpp"
#include "lms/User.hpp"

namespace lms {

struct AssignmentView {
    Assignment assignment;
    std::string courseCode;
    std::optional<SubmissionStatus> submissionStatus;  // only filled for students
};

struct SubmissionReceipt {
    Id submissionId = 0;
    int attemptNo = 1;
    SubmissionStatus status = SubmissionStatus::Submitted;
    int daysLate = 0;
    double penaltyPercent = 0;
    std::string submittedAt;
    std::string effectiveDeadline;
};

// Assignments, extensions and submissions (FR-06 to FR-11, UC-01).
class CourseworkService {
public:
    CourseworkService(ICourseRepository& courses, IEnrolmentRepository& enrolments, IAssignmentRepository& assignments,
                      IExtensionRepository& extensions, ISubmissionRepository& submissions, IContentStore& content,
                      const ILatePenaltyStrategy& penalty, const IClock& clock, ITransactionRunner& tx);

    Id createAssignment(const User& lecturer, Id courseId, const std::string& title, const std::string& description,
                        const std::string& deadline, double maxMark);
    Id grantExtension(const User& lecturer, Id assignmentId, Id studentId, const std::string& newDeadline, const std::string& reason);

    // The deadline that applies to this student: the original one, or a later approved extension.
    std::string effectiveDeadline(const Assignment& assignment, Id studentId) const;

    // Courses/assignments the user may see: enrolled (student), assigned (lecturer) or departmental (HoD).
    std::vector<Course> coursesFor(const User& user) const;
    std::vector<AssignmentView> assignmentsFor(const User& user, const std::string& keyword = "") const;
    Assignment requireAssignment(Id assignmentId) const;

    SubmissionReceipt submit(const User& student, Id assignmentId, const std::string& sourceFile);
    std::vector<Submission> history(const User& student, Id assignmentId) const;

private:
    ICourseRepository& courses_;
    IEnrolmentRepository& enrolments_;
    IAssignmentRepository& assignments_;
    IExtensionRepository& extensions_;
    ISubmissionRepository& submissions_;
    IContentStore& content_;
    const ILatePenaltyStrategy& penalty_;
    const IClock& clock_;
    ITransactionRunner& tx_;
};

}  // namespace lms
