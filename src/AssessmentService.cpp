#include "lms/AssessmentService.hpp"

#include "lms/Validation.hpp"

namespace lms {

AssessmentService::AssessmentService(IUserRepository& users, ICourseRepository& courses, IAssignmentRepository& assignments,
                                     ISubmissionRepository& submissions, IMarkRepository& marks, IAuditRepository& audit,
                                     const ILatePenaltyStrategy& penalty, const IClock& clock, ITransactionRunner& tx)
    : users_(users), courses_(courses), assignments_(assignments), submissions_(submissions), marks_(marks),
      audit_(audit), penalty_(penalty), clock_(clock), tx_(tx) {}

// ------------------------------------------------------------------ helpers -------------------
AssessmentService::Context AssessmentService::load(Id submissionId) const {
    auto submission = submissions_.findById(submissionId);
    if (!submission) throw NotFoundError("Submission #" + std::to_string(submissionId) + " does not exist.");
    auto assignment = assignments_.findById(submission->assignmentId);
    if (!assignment) throw NotFoundError("The assignment for this submission no longer exists.");
    auto course = courses_.findById(assignment->courseId);
    if (!course) throw NotFoundError("The course for this submission no longer exists.");
    return Context{*submission, *assignment, *course};
}

void AssessmentService::requireLecturerOf(const User& lecturer, const Context& ctx) const {
    if (!courses_.isLecturerAssigned(ctx.course.id, lecturer.id())) throw PermissionError("You are not assigned to this course.");
}

void AssessmentService::requireHodOf(const User& hod, const Context& ctx) const {
    if (!hod.departmentId() || *hod.departmentId() != ctx.course.departmentId) {
        throw PermissionError("This submission belongs to another department.");
    }
}

Mark AssessmentService::loadMark(Id submissionId) const {
    auto mark = marks_.findBySubmission(submissionId);
    if (!mark) throw NotFoundError("This submission has not been marked yet.");
    return *mark;
}

std::string AssessmentService::describe(const Mark& mark) {
    return "raw=" + formatMark(mark.rawMark) + ", final=" + formatMark(mark.finalMark);
}

void AssessmentService::writeAudit(Id markId, Id actor, AuditEvent event, const std::optional<std::string>& oldValue,
                                   const std::optional<std::string>& newValue, const std::string& reason) {
    AuditEntry entry;
    entry.markId = markId;
    entry.changedBy = actor;
    entry.changedAt = clock_.nowString();
    entry.event = event;
    entry.oldValue = oldValue;
    entry.newValue = newValue;
    entry.reason = reason;
    audit_.insert(entry);
}

void AssessmentService::changeStatus(Submission& submission, SubmissionStatus to) {
    SubmissionStateMachine::require(submission.status, to);
    submission.status = to;
    submissions_.update(submission);
}

SubmissionView AssessmentService::buildView(const Submission& submission, bool includeMark) const {
    SubmissionView view;
    view.submission = submission;
    if (auto a = assignments_.findById(submission.assignmentId)) {
        view.assignmentTitle = a->title;
        view.maxMark = a->maxMark;
        if (auto c = courses_.findById(a->courseId)) view.courseCode = c->code;
    }
    if (auto s = users_.findById(submission.studentId)) {
        view.studentName = s->fullName;
        view.studentUsername = s->username;
    }
    if (includeMark) view.mark = marks_.findBySubmission(submission.id);
    return view;
}

std::vector<SubmissionView> AssessmentService::filtered(const std::vector<Submission>& list, const SubmissionFilter& filter) const {
    std::vector<SubmissionView> out;
    const std::string needle = trim(filter.keyword);
    for (const auto& s : list) {
        if (filter.status && s.status != *filter.status) continue;
        SubmissionView view = buildView(s, true);
        if (!needle.empty() && !containsIgnoreCase(view.assignmentTitle, needle) && !containsIgnoreCase(view.courseCode, needle) &&
            !containsIgnoreCase(view.studentName, needle) && !containsIgnoreCase(view.studentUsername, needle)) {
            continue;
        }
        out.push_back(std::move(view));
    }
    return out;
}

// ------------------------------------------------------------------ lecturer ------------------
std::vector<SubmissionView> AssessmentService::submissionsForCourse(const User& lecturer, Id courseId, const SubmissionFilter& filter) const {
    requirePermission(lecturer, Permission::MarkSubmissions);
    if (!courses_.isLecturerAssigned(courseId, lecturer.id())) throw PermissionError("You are not assigned to this course.");
    return filtered(submissions_.latestByCourse(courseId, std::nullopt), filter);
}

void AssessmentService::recordMark(const User& lecturer, Id submissionId, double rawMark, const std::string& feedback, const std::string& reason) {
    requirePermission(lecturer, Permission::MarkSubmissions);
    Context ctx = load(submissionId);
    requireLecturerOf(lecturer, ctx);

    // Only the student's latest attempt is marked, and only before moderation has started.
    auto latest = submissions_.findLatest(ctx.submission.assignmentId, ctx.submission.studentId);
    if (!latest || latest->id != ctx.submission.id) throw BusinessRuleError("Only the student's latest attempt can be marked.");
    const SubmissionStatus status = ctx.submission.status;
    if (status != SubmissionStatus::Submitted && status != SubmissionStatus::Late && status != SubmissionStatus::Marked &&
        status != SubmissionStatus::ReturnedForCorrection) {
        throw BusinessRuleError("This submission is '" + displayName(status) + "' and its mark can no longer be changed by the lecturer.");
    }

    const double raw = lms::requireMark(rawMark, ctx.assignment.maxMark, "Mark");
    const std::string cleanFeedback = requireText(feedback, "Feedback", 2000);
    auto existing = marks_.findBySubmission(submissionId);
    const std::string cleanReason = existing ? requireText(reason, "Reason for changing the mark", 500)
                                             : (trim(reason).empty() ? std::string("Initial marking") : requireText(reason, "Reason", 500));

    tx_.runInTransaction([&] {
        const double penaltyPercent = penalty_.penaltyPercent(ctx.submission.daysLate);
        Mark mark = existing ? *existing : Mark{};
        const std::optional<std::string> before = existing ? std::optional<std::string>(describe(*existing)) : std::nullopt;
        mark.submissionId = submissionId;
        mark.rawMark = raw;
        mark.penaltyPercent = penaltyPercent;
        mark.finalMark = penalty_.apply(raw, ctx.assignment.maxMark, ctx.submission.daysLate);
        mark.feedback = cleanFeedback;
        mark.markedBy = lecturer.id();
        mark.markedAt = clock_.nowString();
        mark.moderationStatus = ModerationStatus::Pending;
        mark.moderatedBy.reset();
        mark.moderatedAt.reset();
        mark.moderationComment.reset();
        if (existing) {
            marks_.update(mark);
        } else {
            mark.id = marks_.insert(mark);
        }
        writeAudit(mark.id, lecturer.id(), existing ? AuditEvent::MarkChanged : AuditEvent::MarkCreated, before, describe(mark), cleanReason);
        changeStatus(ctx.submission, SubmissionStatus::Marked);
    });
}

void AssessmentService::submitForModeration(const User& lecturer, Id submissionId) {
    requirePermission(lecturer, Permission::MarkSubmissions);
    Context ctx = load(submissionId);
    requireLecturerOf(lecturer, ctx);
    Mark mark = loadMark(submissionId);
    tx_.runInTransaction([&] {
        changeStatus(ctx.submission, SubmissionStatus::AwaitingModeration);
        writeAudit(mark.id, lecturer.id(), AuditEvent::SubmittedForModeration, std::nullopt, describe(mark), "Sent to Head of Department for moderation");
    });
}

// ------------------------------------------------------------------ HoD -----------------------
std::vector<SubmissionView> AssessmentService::moderationQueue(const User& hod, const SubmissionFilter& filter) const {
    requirePermission(hod, Permission::ModerateMarks);
    if (!hod.departmentId()) throw PermissionError("Your account has no department.");
    SubmissionFilter f = filter;
    f.status = SubmissionStatus::AwaitingModeration;
    return filtered(submissions_.latestByDepartment(*hod.departmentId(), SubmissionStatus::AwaitingModeration), f);
}

std::vector<SubmissionView> AssessmentService::approvedAwaitingRelease(const User& hod) const {
    requirePermission(hod, Permission::ReleaseMarks);
    if (!hod.departmentId()) throw PermissionError("Your account has no department.");
    return filtered(submissions_.latestByDepartment(*hod.departmentId(), SubmissionStatus::Moderated), SubmissionFilter{});
}

void AssessmentService::adjustMark(const User& hod, Id submissionId, double newRawMark, const std::string& reason) {
    requirePermission(hod, Permission::ModerateMarks);
    Context ctx = load(submissionId);
    requireHodOf(hod, ctx);
    if (ctx.submission.status != SubmissionStatus::AwaitingModeration) throw BusinessRuleError("Only marks awaiting moderation can be adjusted.");
    const double raw = lms::requireMark(newRawMark, ctx.assignment.maxMark, "Mark");
    const std::string cleanReason = requireText(reason, "Reason for adjusting the mark", 500);
    Mark mark = loadMark(submissionId);
    tx_.runInTransaction([&] {
        const std::string before = describe(mark);
        mark.rawMark = raw;
        mark.finalMark = penalty_.apply(raw, ctx.assignment.maxMark, ctx.submission.daysLate);
        mark.markedAt = clock_.nowString();
        marks_.update(mark);
        writeAudit(mark.id, hod.id(), AuditEvent::MarkChanged, before, describe(mark), cleanReason);
    });
}

void AssessmentService::approve(const User& hod, Id submissionId, const std::string& comment) {
    requirePermission(hod, Permission::ModerateMarks);
    Context ctx = load(submissionId);
    requireHodOf(hod, ctx);
    Mark mark = loadMark(submissionId);
    const std::string note = trim(comment).empty() ? std::string("Approved by Head of Department") : requireText(comment, "Comment", 500);
    tx_.runInTransaction([&] {
        changeStatus(ctx.submission, SubmissionStatus::Moderated);
        mark.moderationStatus = ModerationStatus::Approved;
        mark.moderatedBy = hod.id();
        mark.moderatedAt = clock_.nowString();
        mark.moderationComment = note;
        marks_.update(mark);
        writeAudit(mark.id, hod.id(), AuditEvent::Approved, std::nullopt, describe(mark), note);
    });
}

void AssessmentService::returnForCorrection(const User& hod, Id submissionId, const std::string& reason) {
    requirePermission(hod, Permission::ModerateMarks);
    Context ctx = load(submissionId);
    requireHodOf(hod, ctx);
    Mark mark = loadMark(submissionId);
    const std::string why = requireText(reason, "Reason for returning the mark", 500);
    tx_.runInTransaction([&] {
        changeStatus(ctx.submission, SubmissionStatus::ReturnedForCorrection);
        mark.moderationStatus = ModerationStatus::Returned;
        mark.moderatedBy = hod.id();
        mark.moderatedAt = clock_.nowString();
        mark.moderationComment = why;
        marks_.update(mark);
        writeAudit(mark.id, hod.id(), AuditEvent::Returned, std::nullopt, describe(mark), why);
    });
}

void AssessmentService::release(const User& hod, Id submissionId) {
    requirePermission(hod, Permission::ReleaseMarks);
    Context ctx = load(submissionId);
    requireHodOf(hod, ctx);
    Mark mark = loadMark(submissionId);
    if (ctx.submission.status != SubmissionStatus::Moderated || mark.moderationStatus != ModerationStatus::Approved) {
        throw BusinessRuleError("Only results that the Head of Department has approved can be released.");
    }
    tx_.runInTransaction([&] {
        changeStatus(ctx.submission, SubmissionStatus::Released);
        mark.releasedBy = hod.id();
        mark.releasedAt = clock_.nowString();
        marks_.update(mark);
        writeAudit(mark.id, hod.id(), AuditEvent::Released, std::nullopt, describe(mark), "Released to student");
    });
}

int AssessmentService::releaseAllApproved(const User& hod) {
    int released = 0;
    tx_.runInTransaction([&] {  // all-or-nothing: a failure releases nothing
        for (const auto& view : approvedAwaitingRelease(hod)) {
            release(hod, view.submission.id);
            ++released;
        }
    });
    return released;
}

// ------------------------------------------------------------------ audit ---------------------
std::vector<AuditView> AssessmentService::auditTrail(const User& actor, Id submissionId) const {
    Context ctx = load(submissionId);
    if (actor.role() == Role::Lecturer) {
        requireLecturerOf(actor, ctx);
    } else if (actor.role() == Role::HoD) {
        requireHodOf(actor, ctx);
    } else {
        throw PermissionError("Only the course lecturer or the Head of Department can view the audit trail.");
    }
    std::vector<AuditView> out;
    if (auto mark = marks_.findBySubmission(submissionId)) {
        for (const auto& entry : audit_.findByMark(mark->id)) {
            auto who = users_.findById(entry.changedBy);
            out.push_back(AuditView{entry, who ? who->fullName : std::string("(unknown)")});
        }
    }
    return out;
}

// ------------------------------------------------------------------ student -------------------
std::vector<SubmissionView> AssessmentService::mySubmissions(const User& student) const {
    requirePermission(student, Permission::ViewReleasedResults);
    std::vector<SubmissionView> out;
    for (const auto& s : submissions_.latestByStudent(student.id())) {
        SubmissionView view = buildView(s, false);
        if (s.status == SubmissionStatus::Released) view.mark = marks_.findBySubmission(s.id);  // FR-15
        out.push_back(std::move(view));
    }
    return out;
}

ResultView AssessmentService::releasedResult(const User& student, Id submissionId) const {
    requirePermission(student, Permission::ViewReleasedResults);
    static const char* const kUnavailable = "No released result is available for that submission.";
    auto submission = submissions_.findById(submissionId);
    // Same message for "not yours", "not found" and "not released": nothing is revealed (NFR-10).
    if (!submission || submission->studentId != student.id() || submission->status != SubmissionStatus::Released) {
        throw PermissionError(kUnavailable);
    }
    auto mark = marks_.findBySubmission(submissionId);
    if (!mark || !mark->releasedAt) throw PermissionError(kUnavailable);
    SubmissionView view = buildView(*submission, false);
    ResultView result;
    result.submissionId = submissionId;
    result.assignmentTitle = view.assignmentTitle;
    result.courseCode = view.courseCode;
    result.maxMark = view.maxMark;
    result.rawMark = mark->rawMark;
    result.penaltyPercent = mark->penaltyPercent;
    result.finalMark = mark->finalMark;
    result.daysLate = submission->daysLate;
    result.feedback = mark->feedback;
    result.releasedAt = *mark->releasedAt;
    return result;
}

}  // namespace lms
