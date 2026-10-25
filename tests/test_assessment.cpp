// Integration and system-level tests for marking, moderation, release and visibility
// (UC-02, UC-03, FR-12..FR-17, NFR-08, NFR-10).
#include "TestSupport.hpp"

using namespace lmstest;

namespace {
// Student submits on time, lecturer marks 80, sends to moderation.
struct Flow {
    Id assignmentId;
    Id submissionId;
};
}  // namespace

class AssessmentFlow : public SystemFixture {
protected:
    Flow submitted(const std::string& when = "2026-11-09 10:00", User* who = nullptr) {
        Id aid = makeAssignment("2026-11-10 12:00");
        auto receipt = submitAt(when, aid, who ? *who : *student);
        return Flow{aid, receipt.submissionId};
    }
    Flow awaitingModeration() {
        Flow f = submitted();
        assessment.recordMark(*lecturer, f.submissionId, 80, "Well structured essay", "");
        assessment.submitForModeration(*lecturer, f.submissionId);
        return f;
    }
    SubmissionStatus statusOf(Id submissionId) { return subRepo.findById(submissionId)->status; }
};

TEST_F(AssessmentFlow, FullLifecycleFromSubmissionToRelease) {
    Flow f = submitted();
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::Submitted);

    assessment.recordMark(*lecturer, f.submissionId, 80, "Well structured essay", "");
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::Marked);
    assessment.submitForModeration(*lecturer, f.submissionId);
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::AwaitingModeration);

    ASSERT_EQ(assessment.moderationQueue(*hod).size(), 1u);
    assessment.approve(*hod, f.submissionId, "Looks fair");
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::Moderated);
    EXPECT_EQ(assessment.approvedAwaitingRelease(*hod).size(), 1u);

    assessment.release(*hod, f.submissionId);
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::Released);

    ResultView r = assessment.releasedResult(*student, f.submissionId);
    EXPECT_NEAR(r.finalMark, 80, 1e-9);
    EXPECT_NEAR(r.maxMark, 100, 1e-9);
    EXPECT_EQ(r.feedback, "Well structured essay");
    EXPECT_EQ(r.courseCode, "CS101");

    // Audit trail: created, sent, approved, released - each with actor, time and reason.
    auto trail = assessment.auditTrail(*hod, f.submissionId);
    ASSERT_EQ(trail.size(), 4u);
    EXPECT_EQ(trail[0].entry.event, AuditEvent::MarkCreated);
    EXPECT_EQ(trail[0].actorName, "Lena Lecturer");
    EXPECT_EQ(trail[3].entry.event, AuditEvent::Released);
    EXPECT_EQ(trail[3].actorName, "Hanna HoD");
    for (const auto& t : trail) {
        EXPECT_FALSE(t.entry.reason.empty());
        EXPECT_FALSE(t.entry.changedAt.empty());
    }
}

TEST_F(AssessmentFlow, StudentCannotSeeMarkBeforeRelease) {
    Flow f = submitted();
    auto expectHidden = [&] {
        EXPECT_THROW(assessment.releasedResult(*student, f.submissionId), PermissionError);
        for (const auto& v : assessment.mySubmissions(*student)) EXPECT_FALSE(v.mark.has_value());
    };
    expectHidden();                                                          // submitted
    assessment.recordMark(*lecturer, f.submissionId, 80, "Good", "");
    expectHidden();                                                          // marked by lecturer (NOT visible yet)
    assessment.submitForModeration(*lecturer, f.submissionId);
    expectHidden();                                                          // awaiting moderation
    assessment.approve(*hod, f.submissionId, "");
    expectHidden();                                                          // approved but not released
    assessment.release(*hod, f.submissionId);
    EXPECT_NO_THROW(assessment.releasedResult(*student, f.submissionId));    // finally visible
    auto mine = assessment.mySubmissions(*student);
    ASSERT_EQ(mine.size(), 1u);
    EXPECT_TRUE(mine[0].mark.has_value());
}

TEST_F(AssessmentFlow, StudentsCannotSeeEachOthersResults) {
    Flow f = awaitingModeration();
    assessment.approve(*hod, f.submissionId, "");
    assessment.release(*hod, f.submissionId);
    EXPECT_THROW(assessment.releasedResult(*student2, f.submissionId), PermissionError);
    EXPECT_THROW(assessment.releasedResult(*student, 99999), PermissionError);  // same message as hidden
    EXPECT_EQ(assessment.mySubmissions(*student2).size(), 0u);
}

TEST_F(AssessmentFlow, LecturerCannotReleaseOrModerate) {
    Flow f = awaitingModeration();
    EXPECT_THROW(assessment.approve(*lecturer, f.submissionId, ""), PermissionError);
    EXPECT_THROW(assessment.release(*lecturer, f.submissionId), PermissionError);
    EXPECT_THROW(assessment.returnForCorrection(*lecturer, f.submissionId, "x"), PermissionError);
    EXPECT_THROW(assessment.moderationQueue(*lecturer), PermissionError);
    EXPECT_THROW(assessment.release(*student, f.submissionId), PermissionError);
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::AwaitingModeration);
}

TEST_F(AssessmentFlow, UnapprovedResultCannotBeReleased) {
    Flow f = awaitingModeration();
    EXPECT_THROW(assessment.release(*hod, f.submissionId), BusinessRuleError);   // awaiting moderation
    Flow g = submitted("2026-11-09 11:00", student2.get());
    EXPECT_THROW(assessment.release(*hod, g.submissionId), NotFoundError);       // no mark at all
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::AwaitingModeration);
}

TEST_F(AssessmentFlow, HodCanOnlyModerateOwnDepartment) {
    Flow f = awaitingModeration();
    EXPECT_THROW(assessment.approve(*otherHod, f.submissionId, ""), PermissionError);
    EXPECT_THROW(assessment.release(*otherHod, f.submissionId), PermissionError);
    EXPECT_THROW(assessment.adjustMark(*otherHod, f.submissionId, 50, "no"), PermissionError);
    EXPECT_EQ(assessment.moderationQueue(*otherHod).size(), 0u);
    EXPECT_THROW(assessment.auditTrail(*otherHod, f.submissionId), PermissionError);
}

TEST_F(AssessmentFlow, ReturnForCorrectionLoop) {
    Flow f = awaitingModeration();
    EXPECT_THROW(assessment.returnForCorrection(*hod, f.submissionId, "  "), ValidationError);  // reason required
    assessment.returnForCorrection(*hod, f.submissionId, "Rubric item 3 not applied");
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::ReturnedForCorrection);
    EXPECT_EQ(assessment.moderationQueue(*hod).size(), 0u);
    EXPECT_THROW(assessment.releasedResult(*student, f.submissionId), PermissionError);          // still hidden

    EXPECT_THROW(assessment.recordMark(*lecturer, f.submissionId, 75, "Re-marked", ""), ValidationError);  // reason needed for change
    assessment.recordMark(*lecturer, f.submissionId, 75, "Re-marked against rubric", "Applied rubric item 3");
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::Marked);
    assessment.submitForModeration(*lecturer, f.submissionId);
    assessment.approve(*hod, f.submissionId, "Correct now");
    assessment.release(*hod, f.submissionId);
    EXPECT_NEAR(assessment.releasedResult(*student, f.submissionId).finalMark, 75, 1e-9);

    // Every mark change is on record with old and new values.
    auto trail = assessment.auditTrail(*lecturer, f.submissionId);
    bool sawChange = false;
    for (const auto& t : trail) {
        if (t.entry.event == AuditEvent::MarkChanged) {
            sawChange = true;
            EXPECT_EQ(t.entry.oldValue.value_or(""), "raw=80.00, final=80.00");
            EXPECT_EQ(t.entry.newValue.value_or(""), "raw=75.00, final=75.00");
            EXPECT_EQ(t.entry.reason, "Applied rubric item 3");
        }
    }
    EXPECT_TRUE(sawChange);
}

TEST_F(AssessmentFlow, HodAdjustmentIsAuditedWithReason) {
    Flow f = awaitingModeration();
    EXPECT_THROW(assessment.adjustMark(*hod, f.submissionId, 70, ""), ValidationError);
    EXPECT_THROW(assessment.adjustMark(*hod, f.submissionId, 101, "too high"), ValidationError);
    assessment.adjustMark(*hod, f.submissionId, 70, "Moderation sample showed over-marking");
    auto mark = markRepo.findBySubmission(f.submissionId);
    EXPECT_NEAR(mark->rawMark, 70, 1e-9);
    auto trail = assessment.auditTrail(*hod, f.submissionId);
    EXPECT_EQ(trail.back().entry.event, AuditEvent::MarkChanged);
    EXPECT_EQ(trail.back().actorName, "Hanna HoD");
    EXPECT_EQ(trail.back().entry.oldValue.value_or(""), "raw=80.00, final=80.00");
    // After release the HoD can no longer change it.
    assessment.approve(*hod, f.submissionId, "");
    EXPECT_THROW(assessment.adjustMark(*hod, f.submissionId, 60, "late change"), BusinessRuleError);
}

TEST_F(AssessmentFlow, MarkValidation) {
    Flow f = submitted();
    EXPECT_THROW(assessment.recordMark(*lecturer, f.submissionId, -1, "x", ""), ValidationError);
    EXPECT_THROW(assessment.recordMark(*lecturer, f.submissionId, 100.5, "x", ""), ValidationError);
    EXPECT_THROW(assessment.recordMark(*lecturer, f.submissionId, 50, "", ""), ValidationError);   // feedback required (UC-02 A2)
    EXPECT_THROW(assessment.recordMark(*lecturer, f.submissionId, 50, std::string(2001, 'x'), ""), ValidationError);
    EXPECT_EQ(markRepo.findAll().size(), 0u);          // nothing saved, no audit noise
    EXPECT_EQ(auditRepo.findAll().size(), 0u);
    EXPECT_NO_THROW(assessment.recordMark(*lecturer, f.submissionId, 0, "Nothing relevant submitted", ""));
    EXPECT_NO_THROW(assessment.recordMark(*lecturer, f.submissionId, 100, "Perfect", "Re-checked, full marks"));
    EXPECT_THROW(assessment.recordMark(*lecturer, 99999, 50, "x", ""), NotFoundError);
}

TEST_F(AssessmentFlow, LatePenaltyIsAppliedToFinalMark) {
    Id aid = makeAssignment("2026-11-10 12:00");
    struct { const char* when; double expectedFinal; } cases[] = {
        {"2026-11-10 12:00:01", 70},   // 1 day  -> -10
        {"2026-11-11 12:00:01", 60},   // 2 days -> -20
        {"2026-11-13 12:00:00", 50},   // 3 days -> -30
        {"2026-11-13 12:00:01", 0}};   // 4 days -> zero
    int n = 0;
    for (auto& c : cases) {
        std::string name = "pen" + std::to_string(++n);
        Id sid = makeUser(name, "Penalty Student", Role::Student, std::nullopt);
        admin.enrolStudent(*adminUser, courseId, sid);
        auto u = login(name, "Password1");
        auto receipt = submitAt(c.when, aid, *u);
        assessment.recordMark(*lecturer, receipt.submissionId, 80, "Marked", "");
        auto mark = markRepo.findBySubmission(receipt.submissionId);
        EXPECT_NEAR(mark->rawMark, 80, 1e-9);
        EXPECT_NEAR(mark->finalMark, c.expectedFinal, 1e-9);
    }
}

TEST_F(AssessmentFlow, LecturerOnlySeesAndMarksOwnCourses) {
    Flow f = submitted();
    EXPECT_THROW(assessment.submissionsForCourse(*lecturer2, courseId), PermissionError);
    EXPECT_THROW(assessment.recordMark(*lecturer2, f.submissionId, 50, "x", ""), PermissionError);
    EXPECT_THROW(assessment.submitForModeration(*lecturer2, f.submissionId), PermissionError);
    EXPECT_THROW(assessment.submissionsForCourse(*student, courseId), PermissionError);
    EXPECT_EQ(assessment.submissionsForCourse(*lecturer, courseId).size(), 1u);
}

TEST_F(AssessmentFlow, OnlyLatestAttemptCanBeMarked) {
    Id aid = makeAssignment();
    auto first = submitAt("2026-11-05 10:00", aid, *student);
    submitAt("2026-11-06 10:00", aid, *student);
    EXPECT_THROW(assessment.recordMark(*lecturer, first.submissionId, 50, "old attempt", ""), BusinessRuleError);
}

TEST_F(AssessmentFlow, WorkflowOrderIsEnforced) {
    Flow f = submitted();
    EXPECT_THROW(assessment.submitForModeration(*lecturer, f.submissionId), NotFoundError);   // not marked yet
    assessment.recordMark(*lecturer, f.submissionId, 60, "ok", "");
    assessment.submitForModeration(*lecturer, f.submissionId);
    EXPECT_THROW(assessment.submitForModeration(*lecturer, f.submissionId), BusinessRuleError);  // twice
    EXPECT_THROW(assessment.recordMark(*lecturer, f.submissionId, 61, "edit", "why"), BusinessRuleError);  // locked
    assessment.approve(*hod, f.submissionId, "");
    EXPECT_THROW(assessment.approve(*hod, f.submissionId, ""), BusinessRuleError);            // already approved
    EXPECT_THROW(assessment.returnForCorrection(*hod, f.submissionId, "too late"), BusinessRuleError);
    assessment.release(*hod, f.submissionId);
    EXPECT_THROW(assessment.release(*hod, f.submissionId), BusinessRuleError);                // already released
}

TEST_F(AssessmentFlow, ReleaseAllApprovedIsDepartmentScoped) {
    Flow a = awaitingModeration();
    Flow b = submitted("2026-11-09 11:00", student2.get());
    assessment.recordMark(*lecturer, b.submissionId, 55, "Fair", "");
    assessment.submitForModeration(*lecturer, b.submissionId);
    assessment.approve(*hod, a.submissionId, "");
    EXPECT_EQ(assessment.releaseAllApproved(*otherHod), 0);       // other department releases nothing
    EXPECT_EQ(assessment.releaseAllApproved(*hod), 1);            // only the approved one
    EXPECT_EQ(statusOf(a.submissionId), SubmissionStatus::Released);
    EXPECT_EQ(statusOf(b.submissionId), SubmissionStatus::AwaitingModeration);
}

TEST_F(AssessmentFlow, SearchAndFilterByStatusAndKeyword) {
    Flow a = awaitingModeration();
    Flow b = submitted("2026-11-09 11:00", student2.get());
    (void)b;
    SubmissionFilter f;
    f.status = SubmissionStatus::AwaitingModeration;
    EXPECT_EQ(assessment.submissionsForCourse(*lecturer, courseId, f).size(), 1u);
    f.status.reset();
    f.keyword = "thabo";
    auto byName = assessment.submissionsForCourse(*lecturer, courseId, f);
    ASSERT_EQ(byName.size(), 1u);
    EXPECT_EQ(byName[0].studentUsername, "stud2");
    f.keyword = "no-such-thing";
    EXPECT_EQ(assessment.submissionsForCourse(*lecturer, courseId, f).size(), 0u);
    SubmissionFilter q;
    q.keyword = "essay";
    EXPECT_EQ(assessment.moderationQueue(*hod, q).size(), 1u);
    (void)a;
}

TEST_F(AssessmentFlow, AuditTrailAccessRules) {
    Flow f = awaitingModeration();
    EXPECT_THROW(assessment.auditTrail(*student, f.submissionId), PermissionError);
    EXPECT_THROW(assessment.auditTrail(*adminUser, f.submissionId), PermissionError);
    EXPECT_THROW(assessment.auditTrail(*lecturer2, f.submissionId), PermissionError);
    EXPECT_EQ(assessment.auditTrail(*lecturer, f.submissionId).size(), 2u);
}

TEST_F(AssessmentFlow, FailedDatabaseWriteLeavesNoPartialChange) {
    Flow f = submitted();
    // Force the audit insert to fail (FK to the actor): the mark and status change must roll back.
    db.execute("PRAGMA foreign_keys = ON;");
    class BrokenAudit : public SqliteAuditRepository {
    public:
        using SqliteAuditRepository::SqliteAuditRepository;
        Id insert(const AuditEntry&) override { throw DatabaseError("disk full"); }
    } broken(db);
    AssessmentService faulty(userRepo, courseRepo, assignRepo, subRepo, markRepo, broken, penalty, clock, db);
    EXPECT_THROW(faulty.recordMark(*lecturer, f.submissionId, 80, "x", ""), DatabaseError);
    EXPECT_EQ(markRepo.findAll().size(), 0u);
    EXPECT_EQ(statusOf(f.submissionId), SubmissionStatus::Submitted);
}
