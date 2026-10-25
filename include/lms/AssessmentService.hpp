#pragma once

#include <vector>

#include "lms/Database.hpp"
#include "lms/LatePenalty.hpp"
#include "lms/Repositories.hpp"
#include "lms/TimeUtil.hpp"
#include "lms/User.hpp"

namespace lms {

struct SubmissionFilter {
    std::optional<SubmissionStatus> status;
    std::string keyword;  // matches assignment title, course code, student name or username
};

struct SubmissionView {
    Submission submission;
    std::string assignmentTitle;
    std::string courseCode;
    std::string studentName;
    std::string studentUsername;
    double maxMark = 0;
    std::optional<Mark> mark;  // hidden from students until released
};

struct ResultView {
    Id submissionId = 0;
    std::string assignmentTitle;
    std::string courseCode;
    double maxMark = 0;
    double rawMark = 0;
    double penaltyPercent = 0;
    double finalMark = 0;
    int daysLate = 0;
    std::string feedback;
    std::string releasedAt;
};

struct AuditView {
    AuditEntry entry;
    std::string actorName;
};

// Marking, moderation, release and result viewing (FR-12 to FR-18; UC-02, UC-03).
// Visibility rule (FR-15): a student can only ever obtain a mark whose submission is Released.
class AssessmentService {
public:
    AssessmentService(IUserRepository& users, ICourseRepository& courses, IAssignmentRepository& assignments,
                      ISubmissionRepository& submissions, IMarkRepository& marks, IAuditRepository& audit,
                      const ILatePenaltyStrategy& penalty, const IClock& clock, ITransactionRunner& tx);

    // ---- Lecturer -------------------------------------------------------------------------
    std::vector<SubmissionView> submissionsForCourse(const User& lecturer, Id courseId, const SubmissionFilter& filter = {}) const;
    // Creates or changes the mark. 'reason' is mandatory when an existing mark is changed.
    void recordMark(const User& lecturer, Id submissionId, double rawMark, const std::string& feedback, const std::string& reason);
    void submitForModeration(const User& lecturer, Id submissionId);

    // ---- Head of Department ---------------------------------------------------------------
    std::vector<SubmissionView> moderationQueue(const User& hod, const SubmissionFilter& filter = {}) const;
    std::vector<SubmissionView> approvedAwaitingRelease(const User& hod) const;
    void adjustMark(const User& hod, Id submissionId, double newRawMark, const std::string& reason);
    void approve(const User& hod, Id submissionId, const std::string& comment);
    void returnForCorrection(const User& hod, Id submissionId, const std::string& reason);
    void release(const User& hod, Id submissionId);
    int releaseAllApproved(const User& hod);

    // ---- Audit trail (lecturer of the course or HoD of its department) --------------------
    std::vector<AuditView> auditTrail(const User& actor, Id submissionId) const;

    // ---- Student --------------------------------------------------------------------------
    std::vector<SubmissionView> mySubmissions(const User& student) const;  // marks only when released
    ResultView releasedResult(const User& student, Id submissionId) const;

private:
    struct Context {
        Submission submission;
        Assignment assignment;
        Course course;
    };
    Context load(Id submissionId) const;
    void requireLecturerOf(const User& lecturer, const Context& ctx) const;
    void requireHodOf(const User& hod, const Context& ctx) const;
    Mark loadMark(Id submissionId) const;
    SubmissionView buildView(const Submission& submission, bool includeMark) const;
    std::vector<SubmissionView> filtered(const std::vector<Submission>& list, const SubmissionFilter& filter) const;
    void writeAudit(Id markId, Id actor, AuditEvent event, const std::optional<std::string>& oldValue,
                    const std::optional<std::string>& newValue, const std::string& reason);
    void changeStatus(Submission& submission, SubmissionStatus to);
    static std::string describe(const Mark& mark);

    IUserRepository& users_;
    ICourseRepository& courses_;
    IAssignmentRepository& assignments_;
    ISubmissionRepository& submissions_;
    IMarkRepository& marks_;
    IAuditRepository& audit_;
    const ILatePenaltyStrategy& penalty_;
    const IClock& clock_;
    ITransactionRunner& tx_;
};

}  // namespace lms
