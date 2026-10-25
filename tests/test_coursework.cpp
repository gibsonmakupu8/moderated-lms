// Integration tests for assignments, extensions and submissions (UC-01, FR-06..FR-11).
#include "TestSupport.hpp"

using namespace lmstest;

TEST_F(SystemFixture, LecturerCreatesAssignmentWithValidation) {
    clock.set("2026-11-02 09:00");
    EXPECT_NO_THROW(makeAssignment());
    EXPECT_THROW(coursework.createAssignment(*lecturer, courseId, "", "d", "2026-11-10 12:00", 100), ValidationError);
    EXPECT_THROW(coursework.createAssignment(*lecturer, courseId, "T", "d", "2026-11-10 12:00", 0), ValidationError);
    EXPECT_THROW(coursework.createAssignment(*lecturer, courseId, "T", "d", "2026-11-10 12:00", -5), ValidationError);
    EXPECT_THROW(coursework.createAssignment(*lecturer, courseId, "T", "d", "2026-11-10 12:00", 1000.5), ValidationError);
    EXPECT_NO_THROW(coursework.createAssignment(*lecturer, courseId, "T", "d", "2026-11-10 12:00", 1000));
    EXPECT_THROW(coursework.createAssignment(*lecturer, courseId, "T", "d", "not a date", 100), ValidationError);
    EXPECT_THROW(coursework.createAssignment(*lecturer, courseId, "T", "d", "2026-11-01 12:00", 100), ValidationError);  // past
    EXPECT_THROW(coursework.createAssignment(*lecturer, courseId, "T", std::string(2001, 'x'), "2026-11-10 12:00", 100), ValidationError);
    EXPECT_THROW(coursework.createAssignment(*lecturer, 999, "T", "d", "2026-11-10 12:00", 100), NotFoundError);
}

TEST_F(SystemFixture, OnlyAssignedLecturersCreateAssignmentsAndExtensions) {
    EXPECT_THROW(coursework.createAssignment(*lecturer2, courseId, "T", "d", "2026-11-10 12:00", 100), PermissionError);  // not assigned
    EXPECT_THROW(coursework.createAssignment(*student, courseId, "T", "d", "2026-11-10 12:00", 100), PermissionError);
    EXPECT_THROW(coursework.createAssignment(*hod, courseId, "T", "d", "2026-11-10 12:00", 100), PermissionError);
    Id aid = makeAssignment();
    EXPECT_THROW(coursework.grantExtension(*lecturer2, aid, studentId, "2026-11-12 12:00", "Illness"), PermissionError);
    EXPECT_THROW(coursework.grantExtension(*student, aid, studentId, "2026-11-12 12:00", "Because"), PermissionError);
}

TEST_F(SystemFixture, ExtensionRulesAndEffectiveDeadline) {
    Id aid = makeAssignment("2026-11-10 12:00");
    Assignment a = coursework.requireAssignment(aid);
    EXPECT_EQ(coursework.effectiveDeadline(a, studentId), "2026-11-10 12:00:00");
    EXPECT_THROW(coursework.grantExtension(*lecturer, aid, studentId, "2026-11-10 12:00", "Same time"), ValidationError);   // not later
    EXPECT_THROW(coursework.grantExtension(*lecturer, aid, studentId, "2026-11-09 12:00", "Earlier"), ValidationError);
    EXPECT_THROW(coursework.grantExtension(*lecturer, aid, studentId, "2026-11-12 12:00", "  "), ValidationError);          // reason
    EXPECT_THROW(coursework.grantExtension(*lecturer, aid, 99999, "2026-11-12 12:00", "Illness"), ValidationError);          // not enrolled
    EXPECT_NO_THROW(coursework.grantExtension(*lecturer, aid, studentId, "2026-11-12 12:00", "Hospital visit"));
    EXPECT_EQ(coursework.effectiveDeadline(a, studentId), "2026-11-12 12:00:00");
    EXPECT_EQ(coursework.effectiveDeadline(a, student2Id), "2026-11-10 12:00:00");  // individual only
    EXPECT_THROW(coursework.grantExtension(*lecturer, aid, studentId, "2026-11-11 12:00", "Shorter"), ValidationError);
    EXPECT_NO_THROW(coursework.grantExtension(*lecturer, aid, studentId, "2026-11-14 12:00", "More time"));
    EXPECT_EQ(coursework.effectiveDeadline(a, studentId), "2026-11-14 12:00:00");
}

TEST_F(SystemFixture, OnTimeSubmissionIsRecordedWithHistory) {
    Id aid = makeAssignment();
    auto r = submitAt("2026-11-09 18:30", aid, *student);
    EXPECT_EQ(r.status, SubmissionStatus::Submitted);
    EXPECT_EQ(r.attemptNo, 1);
    EXPECT_EQ(r.daysLate, 0);
    EXPECT_NEAR(r.penaltyPercent, 0, 1e-9);
    auto stored = subRepo.findById(r.submissionId);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->submittedAt, "2026-11-09 18:30:00");
    EXPECT_TRUE(std::filesystem::exists(stored->contentPath));
}

TEST_F(SystemFixture, ResubmissionBeforeDeadlineKeepsAllAttempts) {
    Id aid = makeAssignment();
    auto first = submitAt("2026-11-05 10:00", aid, *student);
    auto second = submitAt("2026-11-06 10:00", aid, *student);
    auto third = submitAt("2026-11-10 12:00", aid, *student);  // exactly at the deadline is still on time
    EXPECT_EQ(first.attemptNo, 1);
    EXPECT_EQ(second.attemptNo, 2);
    EXPECT_EQ(third.attemptNo, 3);
    EXPECT_EQ(third.status, SubmissionStatus::Submitted);
    EXPECT_EQ(coursework.history(*student, aid).size(), 3u);
    EXPECT_EQ(subRepo.findLatest(aid, studentId)->id, third.submissionId);
}

TEST_F(SystemFixture, LateSubmissionsAreAcceptedWithPenaltyInformation) {
    Id aid = makeAssignment("2026-11-10 12:00");
    struct { const char* when; int days; double pct; } cases[] = {
        {"2026-11-10 12:00:01", 1, 10}, {"2026-11-11 12:00:00", 1, 10}, {"2026-11-11 12:00:01", 2, 20}, {"2026-11-13 12:00:00", 3, 30}};
    int n = 0;
    for (auto& c : cases) {
        // use a fresh student per case so each is a first submission
        std::string name = "late" + std::to_string(++n);
        Id sid = makeUser(name, "Late Student", Role::Student, std::nullopt);
        admin.enrolStudent(*adminUser, courseId, sid);
        auto u = login(name, "Password1");
        auto r = submitAt(c.when, aid, *u);
        EXPECT_EQ(r.status, SubmissionStatus::Late);
        EXPECT_EQ(r.daysLate, c.days);
        EXPECT_NEAR(r.penaltyPercent, c.pct, 1e-9);
    }
}

TEST_F(SystemFixture, MoreThanThreeDaysLateIsStoredWithZeroPenaltyRule) {
    Id aid = makeAssignment("2026-11-10 12:00");
    auto r = submitAt("2026-11-13 12:00:01", aid, *student);  // 72 h + 1 s
    EXPECT_EQ(r.status, SubmissionStatus::Late);               // attempt preserved, not rejected
    EXPECT_EQ(r.daysLate, 4);
    EXPECT_NEAR(r.penaltyPercent, 100, 1e-9);
    ASSERT_TRUE(subRepo.findById(r.submissionId).has_value());
}

TEST_F(SystemFixture, ExtensionMakesLateWorkOnTime) {
    Id aid = makeAssignment("2026-11-10 12:00");
    coursework.grantExtension(*lecturer, aid, studentId, "2026-11-12 12:00", "Family emergency");
    auto r = submitAt("2026-11-11 15:00", aid, *student);
    EXPECT_EQ(r.status, SubmissionStatus::Submitted);
    EXPECT_EQ(r.effectiveDeadline, "2026-11-12 12:00:00");
    // Another student without an extension is late at the same moment.
    auto r2 = submitAt("2026-11-11 15:00", aid, *student2);
    EXPECT_EQ(r2.status, SubmissionStatus::Late);
    EXPECT_EQ(r2.daysLate, 2);
}

TEST_F(SystemFixture, ResubmissionAfterDeadlineIsRefused) {
    Id aid = makeAssignment("2026-11-10 12:00");
    submitAt("2026-11-09 10:00", aid, *student);
    EXPECT_THROW(submitAt("2026-11-10 12:00:01", aid, *student), BusinessRuleError);
    EXPECT_EQ(coursework.history(*student, aid).size(), 1u);  // nothing changed
    // A late first submission also cannot be replaced.
    submitAt("2026-11-11 10:00", aid, *student2);
    EXPECT_THROW(submitAt("2026-11-11 11:00", aid, *student2), BusinessRuleError);
}

TEST_F(SystemFixture, MarkedWorkCannotBeResubmitted) {
    Id aid = makeAssignment();
    auto r = submitAt("2026-11-05 10:00", aid, *student);
    assessment.recordMark(*lecturer, r.submissionId, 60, "OK", "");
    EXPECT_THROW(submitAt("2026-11-06 10:00", aid, *student), BusinessRuleError);
}

TEST_F(SystemFixture, SubmissionRejectsBadFilesAndUnauthorisedUsers) {
    Id aid = makeAssignment();
    clock.set("2026-11-05 10:00");
    EXPECT_THROW(coursework.submit(*student, aid, "/no/such/file.txt"), ValidationError);
    EXPECT_THROW(coursework.submit(*student, aid, ""), ValidationError);
    EXPECT_THROW(coursework.submit(*student, aid, makeFile("empty.txt", "")), ValidationError);
    EXPECT_THROW(coursework.submit(*student, aid, makeFile("virus.exe", "MZ")), ValidationError);
    EXPECT_THROW(coursework.submit(*student, aid, storageDir.string()), ValidationError);  // a folder
    EXPECT_THROW(coursework.submit(*student, 999, makeFile()), NotFoundError);
    EXPECT_THROW(coursework.submit(*lecturer, aid, makeFile()), PermissionError);
    EXPECT_THROW(coursework.history(*lecturer, aid), PermissionError);
    EXPECT_EQ(subRepo.findAll().size(), 0u);  // nothing was recorded

    // A student of another course is not enrolled.
    Id outsider = makeUser("outsider", "Out Sider", Role::Student, std::nullopt);
    (void)outsider;
    auto u = login("outsider", "Password1");
    EXPECT_THROW(coursework.submit(*u, aid, makeFile()), PermissionError);
}

TEST_F(SystemFixture, OversizedFileIsRejected) {
    Id aid = makeAssignment();
    clock.set("2026-11-05 10:00");
    auto path = storageDir / "big.txt";
    std::ofstream out(path, std::ios::binary);
    std::string chunk(1024 * 1024, 'x');
    for (int i = 0; i < 11; ++i) out << chunk;
    out.close();
    EXPECT_THROW(coursework.submit(*student, aid, path.string()), ValidationError);
}

TEST_F(SystemFixture, VisibleCoursesAndAssignmentsFollowTheRole) {
    Id aid = makeAssignment();
    coursework.createAssignment(*lecturer, courseId, "Lab 2", "Lab", "2026-11-20 12:00", 20);
    EXPECT_EQ(coursework.assignmentsFor(*student).size(), 2u);
    EXPECT_EQ(coursework.assignmentsFor(*lecturer).size(), 2u);
    EXPECT_EQ(coursework.assignmentsFor(*hod).size(), 2u);
    EXPECT_EQ(coursework.assignmentsFor(*otherHod).size(), 0u);   // other department
    EXPECT_EQ(coursework.assignmentsFor(*lecturer2).size(), 0u);  // not assigned
    EXPECT_EQ(coursework.assignmentsFor(*student, "lab").size(), 1u);  // keyword filter (FR-18)
    EXPECT_EQ(coursework.assignmentsFor(*student, "cs101").size(), 2u);
    EXPECT_THROW(coursework.assignmentsFor(*adminUser), PermissionError);
    EXPECT_EQ(coursework.coursesFor(*student).size(), 1u);
    EXPECT_EQ(coursework.coursesFor(*hod).size(), 1u);
    EXPECT_EQ(coursework.coursesFor(*lecturer).size(), 1u);
    EXPECT_THROW(coursework.coursesFor(*adminUser), PermissionError);
    submitAt("2026-11-05 10:00", aid, *student);
    auto views = coursework.assignmentsFor(*student);
    int withStatus = 0;
    for (auto& v : views) withStatus += v.submissionStatus.has_value() ? 1 : 0;
    EXPECT_EQ(withStatus, 1);
}
